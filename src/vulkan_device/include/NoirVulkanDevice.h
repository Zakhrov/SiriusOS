#pragma once
#include <vulkan/vulkan.h>
#include <xcb/xcb.h>

// Device selection shared by everything in the Noir stack that renders with
// Vulkan (NoirWM's compositor, NoirShell), so that they all land on the same
// GPU whatever order the loader lists the devices in.

#ifdef __cplusplus
extern "C" {
#endif

// The Vulkan version the Noir stack targets: 1.0, for the widest range of
// hardware. Devices below it are skipped.
#define NOIR_VK_API_VERSION VK_API_VERSION_1_0

// Instance extension to enable when the loader offers it: it lets the X
// server's own GPU be recognised (rule 2 below). Without it that rule is
// skipped.
#define NOIR_VK_INSTANCE_EXTENSION VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME

// Whether the loader offers NOIR_VK_INSTANCE_EXTENSION.
int noir_vk_has_instance_extension(void);

// Says whether queue family `family` of `device` can present to the caller's
// target (a surface, or a window of the X server).
typedef int (*NoirVkPresentQuery)(VkPhysicalDevice device, uint32_t family, void* user);

typedef struct {
    VkPhysicalDevice device;
    uint32_t queueFamily;                // supports graphics and presentation
    VkPhysicalDeviceProperties properties;
    const char* reason;                  // why this device won, for the log
} NoirVkChoice;

// Among the devices of `instance` that support NOIR_VK_API_VERSION and can
// render and present, picks, in order of preference:
//   1. the one NOIR_VK_DEVICE names (a substring of the device name, or an
//      index in enumeration order);
//   2. the one driving the X server, as its DRI3 device node tells (needs
//      NOIR_VK_INSTANCE_EXTENSION enabled on `instance`);
//   3. the best device type (discrete, integrated, virtual, other, CPU),
//      then the lowest vendor and device ID.
// Returns 0 (after printing why, prefixed with `who`) when none qualifies.
int noir_vk_choose_device(VkInstance instance, xcb_connection_t* connection, xcb_window_t root,
                          NoirVkPresentQuery presents, void* user, const char* who,
                          NoirVkChoice* choice);

#ifdef __cplusplus
}
#endif
