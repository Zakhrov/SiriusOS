#include "AppModel.h"
#include "DebugInterface.h"
#include "NotificationServer.h"
#include "TaskModel.h"
#include "X11Integration.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSize>

#include <vector>
#include <vulkan/vulkan.h>

namespace {

// Mesa's hardware Vulkan drivers can only present to an X server that speaks
// DRI3, which virtual servers (Xvfb, Xvnc) do not.
bool serverCanPresentVulkan()
{
    xcb_connection_t *connection = X11Integration::instance()->connection();
    if (!connection)
        return false;
    xcb_query_extension_reply_t *reply = xcb_query_extension_reply(
        connection, xcb_query_extension(connection, 4, "DRI3"), nullptr);
    const bool present = reply && reply->present;
    free(reply);
    return present;
}

// EWMH: a compositing manager owns the _NET_WM_CM_Sn selection of its screen.
bool compositingManagerRunning()
{
    X11Integration *x11 = X11Integration::instance();
    if (!x11->isAvailable())
        return false;
    xcb_get_selection_owner_reply_t *reply = xcb_get_selection_owner_reply(
        x11->connection(), xcb_get_selection_owner(x11->connection(), x11->atom("_NET_WM_CM_S0")), nullptr);
    const bool running = reply && reply->owner != XCB_WINDOW_NONE;
    free(reply);
    return running;
}

struct VulkanDevices {
    bool hardware = false;
    bool software = false;   // a CPU implementation such as lavapipe
};

VulkanDevices probeVulkanDevices()
{
    VulkanDevices found;
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        return found;

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    if (count && vkEnumeratePhysicalDevices(instance, &count, devices.data()) >= VK_SUCCESS) {
        for (uint32_t i = 0; i < count; ++i) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(devices[i], &properties);
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
                found.software = true;
            else
                found.hardware = true;
        }
    }
    vkDestroyInstance(instance, nullptr);
    return found;
}

// The Noir stack is Vulkan only; Qt Quick renders through its Vulkan RHI
// unless the developer overrides it (--backend or QSG_RHI_BACKEND) or Vulkan
// cannot do the job on this X server. `composited` says a compositing manager
// is running, i.e. translucent surfaces are wanted; on return `*translucent`
// says whether the chosen renderer can provide them. Returns false for an
// unknown backend name.
bool selectBackend(QString name, bool explicitChoice, bool composited, bool *translucent)
{
    *translucent = composited;
    if (!explicitChoice && !qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND"))
        return true;

    const bool automatic = name == QLatin1String("auto");
    bool softwareVulkan = false;
    if (automatic || name == QLatin1String("vulkan")) {
        // Hardware drivers present through DRI3. Software ones (lavapipe) use
        // plain X requests, so they also work on Xvfb or in a VM, but Mesa's
        // software presentation cannot target ARGB windows.
        const VulkanDevices devices = probeVulkanDevices();
        const bool hardware = devices.hardware && serverCanPresentVulkan();
        softwareVulkan = !hardware && devices.software;
        if (automatic) {
            name = QStringLiteral("vulkan");
            if (hardware) {
                qInfo("NoirShell: rendering with Vulkan");
            } else if (softwareVulkan && !composited) {
                qInfo("NoirShell: rendering with Vulkan on a software device");
            } else if (softwareVulkan) {
                softwareVulkan = false;
                name = QStringLiteral("opengl");
                qInfo("NoirShell: rendering with OpenGL: software Vulkan cannot present the "
                      "translucent surfaces a compositor expects (force it with --backend vulkan)");
            } else {
                name = QStringLiteral("opengl");
                qWarning("NoirShell: no Vulkan device can present to this X server (no DRI3, no "
                         "software driver); rendering with OpenGL (override with --backend)");
            }
        }
    }
    if (!automatic) {
        qunsetenv("QSG_RHI_BACKEND");
        qInfo("NoirShell: rendering with %s (--backend)", qPrintable(name));
    }
    if (softwareVulkan) {
        // Make Qt pick the CPU device even when a GPU is listed first.
        qputenv("QSG_RHI_PREFER_SOFTWARE_RENDERER", "1");
        // A software swapchain does not block on vsync, which the threaded
        // render loop relies on for pacing; the basic loop runs off a timer.
        if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP"))
            qputenv("QSG_RENDER_LOOP", "basic");
        *translucent = false;
    }

    if (name == QLatin1String("vulkan"))
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    else if (name == QLatin1String("opengl"))
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    else if (name == QLatin1String("software"))
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    else
        return false;
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    // X11 only, by design.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "xcb");

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("NoirShell"));
    QGuiApplication::setOrganizationName(QStringLiteral("SiriusOS"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1"));
    QGuiApplication::setQuitOnLastWindowClosed(true);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SiriusOS Noir desktop shell"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption windowedOption(QStringLiteral("windowed"),
        QStringLiteral("Run the whole shell inside a single window of <size> (e.g. 1280x720) for development."),
        QStringLiteral("size"));
    QCommandLineOption demoOption(QStringLiteral("demo"),
        QStringLiteral("Populate the taskbar and notifications with sample data."));
    QCommandLineOption backendOption(QStringLiteral("backend"),
        QStringLiteral("Renderer: vulkan, opengl, software, or auto (Vulkan, or OpenGL when the X server cannot present it)."),
        QStringLiteral("name"), QStringLiteral("auto"));
    QCommandLineOption debugOption(QStringLiteral("debug-interface"),
        QStringLiteral("Export org.siriusos.NoirShell.Debug on the session bus (item tree, grabs, QML eval)."));
    parser.addOption(windowedOption);
    parser.addOption(demoOption);
    parser.addOption(backendOption);
    parser.addOption(debugOption);
    parser.process(app);

    // Popups and notifications are translucent surfaces, which only a
    // compositing manager can blend. Without one an ARGB window gains nothing
    // (its alpha is ignored), so the surfaces stay opaque.
    bool composited = false;
    if (!selectBackend(parser.value(backendOption).toLower(), parser.isSet(backendOption),
                       compositingManagerRunning(), &composited)) {
        qCritical("NoirShell: unknown backend \"%s\"", qPrintable(parser.value(backendOption)));
        return EXIT_FAILURE;
    }
    // Surfaces opt in to an ARGB visual one by one, by asking for a
    // transparent colour (see shellComposited in SessionShell.qml). The
    // desktop never does: it is the opaque bottom layer.
    QQuickWindow::setDefaultAlphaBuffer(false);

    QSize windowedSize;
    if (parser.isSet(windowedOption)) {
        const QStringList dims = parser.value(windowedOption).split(QLatin1Char('x'));
        windowedSize = QSize(dims.value(0).toInt(), dims.value(1).toInt());
        if (windowedSize.isEmpty())
            windowedSize = QSize(1280, 720);
    }

    // Instantiate the service singletons eagerly so the notification daemon is
    // on the bus before any QML has loaded.
    X11Integration::instance();
    NotificationServer::instance();

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("windowicon"), new WindowIconProvider);
    engine.addImageProvider(QStringLiteral("appicon"), new AppIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("shellWindowed"), windowedSize.isValid());
    engine.rootContext()->setContextProperty(QStringLiteral("shellWindowedSize"), windowedSize);
    engine.rootContext()->setContextProperty(QStringLiteral("shellDemo"), parser.isSet(demoOption));
    engine.rootContext()->setContextProperty(QStringLiteral("shellComposited"), composited);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.load(QUrl(QStringLiteral("qrc:/Noir/Shell/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    if (parser.isSet(debugOption)) {
        auto *debug = new DebugInterface(&engine, &app);
        if (!debug->registerOnBus())
            qWarning("NoirShell: could not export the debug interface on the session bus");
    }

    return app.exec();
}
