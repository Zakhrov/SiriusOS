#include "GraphicsConfig.h"

#include "NoirVulkanDevice.h"
#include "Platform.h"

#include <QFile>
#include <QStringList>

#include <vector>

namespace {

QString typeName(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return QStringLiteral("Discrete GPU");
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return QStringLiteral("Integrated GPU");
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return QStringLiteral("Virtual GPU");
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return QStringLiteral("Software (CPU)");
    default: return QStringLiteral("Other");
    }
}

QString vendorName(uint32_t id)
{
    switch (id) {
    case 0x1002: return QStringLiteral("AMD");
    case 0x10de: return QStringLiteral("NVIDIA");
    case 0x8086: return QStringLiteral("Intel");
    case 0x13b5: return QStringLiteral("Arm");
    case 0x5143: return QStringLiteral("Qualcomm");
    case 0x1af4: return QStringLiteral("Red Hat (virtio)");
    case 0x10005: return QStringLiteral("Mesa");
    default: return QStringLiteral("0x%1").arg(id, 4, 16, QLatin1Char('0'));
    }
}

QString version(uint32_t v)
{
    return QStringLiteral("%1.%2.%3").arg(VK_API_VERSION_MAJOR(v)).arg(VK_API_VERSION_MINOR(v)).arg(VK_API_VERSION_PATCH(v));
}

bool hasDeviceExtension(VkPhysicalDevice device, const char *name)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    for (const VkExtensionProperties &e : extensions) {
        if (strcmp(e.extensionName, name) == 0)
            return true;
    }
    return false;
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

} // namespace

GraphicsConfig::GraphicsConfig(QObject *parent)
    : QObject(parent)
{
    refresh();
}

QVariantList GraphicsConfig::enumerateDevices(QString *activeName, QString *activeReason)
{
    QVariantList result;
    const bool props2 = noir_vk_has_instance_extension();
    std::vector<const char *> extensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XCB_SURFACE_EXTENSION_NAME };
    if (props2)
        extensions.push_back(NOIR_VK_INSTANCE_EXTENSION);
    VkApplicationInfo app = {};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "NoirControlCenter";
    app.apiVersion = NOIR_VK_API_VERSION;
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = uint32_t(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&info, nullptr, &instance) != VK_SUCCESS)
        return result;

    auto getProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2KHR>(
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR"));

    // The device the stack would pick, by the same rules NoirWM and NoirShell use.
    xcb_connection_t *connection = Platform::xConnection();
    xcb_screen_t *screen = Platform::xScreen();
    NoirVkChoice choice = {};
    bool chosen = false;
    PresentTarget target = { connection, screen ? screen->root_visual : 0 };
    if (connection && screen)
        chosen = noir_vk_choose_device(instance, connection, screen->root, presentsToX, &target,
                                       "NoirControlCenter", &choice);

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        QVariantMap entry;
        entry[QStringLiteral("name")] = QString::fromUtf8(properties.deviceName);
        entry[QStringLiteral("type")] = int(properties.deviceType);
        entry[QStringLiteral("typeName")] = typeName(properties.deviceType);
        entry[QStringLiteral("vendor")] = vendorName(properties.vendorID);
        entry[QStringLiteral("deviceId")] = QStringLiteral("%1:%2").arg(properties.vendorID, 4, 16, QLatin1Char('0'))
                                                                  .arg(properties.deviceID, 4, 16, QLatin1Char('0'));
        entry[QStringLiteral("apiVersion")] = version(properties.apiVersion);

        QString driver = QStringLiteral("Unknown driver");
        QString driverInfo;
        if (getProperties2 && hasDeviceExtension(device, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
            VkPhysicalDeviceDriverPropertiesKHR driverProps = {};
            driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR;
            VkPhysicalDeviceProperties2KHR props = {};
            props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
            props.pNext = &driverProps;
            getProperties2(device, &props);
            driver = QString::fromUtf8(driverProps.driverName);
            driverInfo = QString::fromUtf8(driverProps.driverInfo);
        }
        entry[QStringLiteral("driverName")] = driver;
        entry[QStringLiteral("driverInfo")] = driverInfo;

        bool presents = false;
        if (connection && screen) {
            uint32_t families = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &families, nullptr);
            for (uint32_t f = 0; f < families && !presents; ++f)
                presents = presentsToX(device, f, &target);
        }
        entry[QStringLiteral("presents")] = presents;
        entry[QStringLiteral("active")] = chosen && choice.device == device;
        result << entry;
    }
    if (chosen) {
        if (activeName)
            *activeName = QString::fromUtf8(choice.properties.deviceName);
        if (activeReason)
            *activeReason = QString::fromUtf8(choice.reason);
    }
    vkDestroyInstance(instance, nullptr);
    return result;
}

void GraphicsConfig::refresh()
{
    m_activeDevice.clear();
    m_activeReason.clear();
    m_devices = enumerateDevices(&m_activeDevice, &m_activeReason);
    m_preferred = Platform::sessionVariable(QStringLiteral("NOIR_VK_DEVICE"));
    m_compositor = !Platform::sessionVariable(QStringLiteral("NOIR_WM_ARGS"))
                        .split(QLatin1Char(' '), Qt::SkipEmptyParts).contains(QStringLiteral("--no-compositor"));
    m_validation = !Platform::sessionVariable(QStringLiteral("NOIR_VK_VALIDATION")).isEmpty();
    emit changed();
}

bool GraphicsConfig::setPreferredDevice(const QString &name)
{
    if (!Platform::setSessionVariable(QStringLiteral("NOIR_VK_DEVICE"), name, true)) {
        emit errorOccurred(QStringLiteral("Could not write ") + sessionConfPath());
        return false;
    }
    refresh();
    return true;
}

bool GraphicsConfig::setCompositorEnabled(bool enabled)
{
    QStringList args = Platform::sessionVariable(QStringLiteral("NOIR_WM_ARGS")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    args.removeAll(QStringLiteral("--no-compositor"));
    if (!enabled)
        args << QStringLiteral("--no-compositor");
    if (!Platform::setSessionVariable(QStringLiteral("NOIR_WM_ARGS"), args.join(QLatin1Char(' ')), false)) {
        emit errorOccurred(QStringLiteral("Could not write ") + sessionConfPath());
        return false;
    }
    refresh();
    return true;
}

bool GraphicsConfig::setValidationEnabled(bool enabled)
{
    if (!Platform::setSessionVariable(QStringLiteral("NOIR_VK_VALIDATION"), enabled ? QStringLiteral("1") : QString(), true)) {
        emit errorOccurred(QStringLiteral("Could not write ") + sessionConfPath());
        return false;
    }
    refresh();
    return true;
}

bool GraphicsConfig::validationAvailable() const
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const VkLayerProperties &layer : layers) {
        if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0)
            return true;
    }
    return false;
}

QString GraphicsConfig::loaderVersion() const
{
    // A Vulkan 1.0 loader does not have the function at all.
    uint32_t v = VK_API_VERSION_1_0;
    auto enumerate = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (enumerate)
        enumerate(&v);
    return version(v);
}

QString GraphicsConfig::sessionConfPath() const
{
    return Platform::sessionConfPath();
}
