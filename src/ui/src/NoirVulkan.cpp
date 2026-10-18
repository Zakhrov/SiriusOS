#include "NoirVulkan.h"

#include "NoirVulkanDevice.h"

#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QQuickGraphicsDevice>
#include <QQuickWindow>
#include <QVulkanInstance>

#include <xcb/xcb.h>

namespace Noir {

namespace {

xcb_connection_t *xConnection()
{
    auto *x11 = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
    return x11 ? x11->connection() : nullptr;
}

xcb_screen_t *xScreen(xcb_connection_t *connection)
{
    return xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
}

// Mesa's hardware Vulkan drivers present through DRI3, which virtual servers
// (Xvfb, Xvnc) lack. There they can only present with MESA_VK_WSI_DEBUG=sw,
// which copies each frame to the X server like a software driver does.
bool serverHasDri3(xcb_connection_t *connection)
{
    xcb_query_extension_reply_t *reply = xcb_query_extension_reply(
        connection, xcb_query_extension(connection, 4, "DRI3"), nullptr);
    const bool present = reply && reply->present;
    free(reply);
    return present;
}

struct PresentTarget {
    xcb_connection_t *connection;
    xcb_visualid_t visual;
};

int presentsToX(VkPhysicalDevice device, uint32_t family, void *user)
{
    const auto *target = static_cast<PresentTarget *>(user);
    return vkGetPhysicalDeviceXcbPresentationSupportKHR(device, family, target->connection, target->visual);
}

// What Qt Quick renders with: a Vulkan 1.0 instance of our own and the device
// the rest of the Noir stack uses. QML creates the windows, so each one is
// handed both when its platform window appears, before its scene graph starts.
class VulkanSetup : public QObject
{
public:
    using QObject::QObject;
    QVulkanInstance instance;
    VkPhysicalDevice device = VK_NULL_HANDLE;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::PlatformSurface
            && static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
            if (auto *window = qobject_cast<QQuickWindow *>(watched)) {
                window->setVulkanInstance(&instance);
                window->setGraphicsDevice(QQuickGraphicsDevice::fromPhysicalDevice(device));
            }
        }
        return false;
    }
};

} // namespace

bool compositingManagerRunning()
{
    xcb_connection_t *connection = xConnection();
    if (!connection)
        return false;
    xcb_intern_atom_reply_t *atom = xcb_intern_atom_reply(
        connection, xcb_intern_atom(connection, 1, 13, "_NET_WM_CM_S0"), nullptr);
    if (!atom)
        return false;
    xcb_get_selection_owner_reply_t *reply = xcb_get_selection_owner_reply(
        connection, xcb_get_selection_owner(connection, atom->atom), nullptr);
    const bool running = reply && reply->owner != XCB_WINDOW_NONE;
    free(reply);
    free(atom);
    return running;
}

bool setUpVulkan(QGuiApplication *app, const char *who, bool composited, bool *translucent)
{
    // No other backend is supported, whatever the environment asks for.
    qunsetenv("QSG_RHI_BACKEND");
    qunsetenv("QT_QUICK_BACKEND");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    xcb_connection_t *connection = xConnection();
    if (!connection) {
        qCritical("%s: no X server", who);
        return false;
    }
    auto *setup = new VulkanSetup(app);
    const QVersionNumber required(VK_API_VERSION_MAJOR(NOIR_VK_API_VERSION), VK_API_VERSION_MINOR(NOIR_VK_API_VERSION));
    setup->instance.setApiVersion(required);
    if (noir_vk_has_instance_extension())
        setup->instance.setExtensions({ NOIR_VK_INSTANCE_EXTENSION });
    if (qEnvironmentVariableIsSet("NOIR_VK_VALIDATION"))
        setup->instance.setLayers({ "VK_LAYER_KHRONOS_validation" });
    if (setup->instance.supportedApiVersion() < required || !setup->instance.create()) {
        qCritical("%s: cannot create a Vulkan %s instance (loader offers %s)", who,
                  qPrintable(required.toString()), qPrintable(setup->instance.supportedApiVersion().toString()));
        return false;
    }

    xcb_screen_t *screen = xScreen(connection);
    PresentTarget target = { connection, screen->root_visual };
    NoirVkChoice choice;
    if (!noir_vk_choose_device(setup->instance.vkInstance(), connection, screen->root,
                               presentsToX, &target, who, &choice))
        return false;
    setup->device = choice.device;
    app->installEventFilter(setup);

    // Software drivers, and hardware ones on a server without DRI3 (Xvfb with
    // MESA_VK_WSI_DEBUG=sw), copy each frame to the X server.
    const bool copied = choice.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU || !serverHasDri3(connection);
    qInfo("%s: rendering with Vulkan %s on %s (%s)%s", who, qPrintable(required.toString()),
          choice.properties.deviceName, choice.reason, copied ? ", frames copied to the X server" : "");
    *translucent = composited && !copied;
    if (copied) {
        // Software presentation does not block on vsync, which the threaded
        // render loop relies on for pacing; the basic loop runs off a timer.
        // It also cannot target ARGB windows, so surfaces stay opaque.
        if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP"))
            qputenv("QSG_RENDER_LOOP", "basic");
        if (composited)
            qInfo("%s: surfaces stay opaque: software presentation cannot target ARGB windows", who);
    }
    return true;
}

} // namespace Noir
