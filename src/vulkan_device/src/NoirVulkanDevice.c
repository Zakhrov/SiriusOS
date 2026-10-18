#define _GNU_SOURCE   // strcasestr
#include "NoirVulkanDevice.h"

#include <xcb/dri3.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

typedef struct {
    VkPhysicalDevice device;
    uint32_t index;          // enumeration order
    uint32_t queueFamily;
    VkPhysicalDeviceProperties properties;
} Candidate;

// The device node the X server renders with, as DRI3 hands it to clients.
static int x_server_device(xcb_connection_t* connection, xcb_window_t root, dev_t* node) {
    if (!connection) {
        return 0;
    }
    const xcb_query_extension_reply_t* dri3 = xcb_get_extension_data(connection, &xcb_dri3_id);
    if (!dri3 || !dri3->present) {
        return 0;
    }
    xcb_dri3_open_reply_t* reply = xcb_dri3_open_reply(connection, xcb_dri3_open(connection, root, 0), NULL);
    if (!reply) {
        return 0;
    }
    int found = 0;
    if (reply->nfd == 1) {
        int fd = xcb_dri3_open_reply_fds(connection, reply)[0];
        struct stat st;
        if (fstat(fd, &st) == 0 && S_ISCHR(st.st_mode)) {
            *node = st.st_rdev;
            found = 1;
        }
        close(fd);
    }
    free(reply);
    return found;
}

static int has_extension(VkPhysicalDevice device, const char* name) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, NULL, &count, NULL);
    VkExtensionProperties* extensions = malloc(sizeof(VkExtensionProperties) * (count ? count : 1));
    vkEnumerateDeviceExtensionProperties(device, NULL, &count, extensions);
    int found = 0;
    for (uint32_t i = 0; i < count && !found; i++) {
        found = strcmp(extensions[i].extensionName, name) == 0;
    }
    free(extensions);
    return found;
}

int noir_vk_has_instance_extension(void) {
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
    VkExtensionProperties* extensions = malloc(sizeof(VkExtensionProperties) * (count ? count : 1));
    vkEnumerateInstanceExtensionProperties(NULL, &count, extensions);
    int found = 0;
    for (uint32_t i = 0; i < count && !found; i++) {
        found = strcmp(extensions[i].extensionName, NOIR_VK_INSTANCE_EXTENSION) == 0;
    }
    free(extensions);
    return found;
}

static int drives_node(PFN_vkGetPhysicalDeviceProperties2KHR getProperties2, VkPhysicalDevice device, dev_t node) {
    if (!has_extension(device, VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME)) {
        return 0;
    }
    VkPhysicalDeviceDrmPropertiesEXT drm = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT };
    VkPhysicalDeviceProperties2 properties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                               .pNext = &drm };
    getProperties2(device, &properties);
    return (drm.hasRender && (dev_t)makedev(drm.renderMajor, drm.renderMinor) == node)
        || (drm.hasPrimary && (dev_t)makedev(drm.primaryMajor, drm.primaryMinor) == node);
}

static int type_rank(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 0;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 1;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 2;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return 4;
    default: return 3;
    }
}

static int better(const Candidate* a, const Candidate* b) {
    int ra = type_rank(a->properties.deviceType), rb = type_rank(b->properties.deviceType);
    if (ra != rb) return ra < rb;
    if (a->properties.vendorID != b->properties.vendorID) return a->properties.vendorID < b->properties.vendorID;
    return a->properties.deviceID < b->properties.deviceID;
}

static int names(const Candidate* c, const char* wanted) {
    const char* p = wanted;
    while (*p && isdigit((unsigned char)*p)) p++;
    if (*wanted && !*p) {
        return c->index == (uint32_t)atoi(wanted);
    }
    return strcasestr(c->properties.deviceName, wanted) != NULL;
}

int noir_vk_choose_device(VkInstance instance, xcb_connection_t* connection, xcb_window_t root,
                          NoirVkPresentQuery presents, void* user, const char* who,
                          NoirVkChoice* choice) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, NULL);
    if (count == 0) {
        fprintf(stderr, "%s: no Vulkan devices\n", who);
        return 0;
    }
    VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * count);
    Candidate* candidates = malloc(sizeof(Candidate) * count);
    vkEnumeratePhysicalDevices(instance, &count, devices);

    uint32_t usable = 0;
    for (uint32_t d = 0; d < count; d++) {
        Candidate c = { .device = devices[d], .index = d, .queueFamily = UINT32_MAX };
        vkGetPhysicalDeviceProperties(devices[d], &c.properties);
        if (c.properties.apiVersion < NOIR_VK_API_VERSION) {
            fprintf(stderr, "%s: skipping %s: Vulkan %u.%u, %u.%u required\n", who, c.properties.deviceName,
                    VK_API_VERSION_MAJOR(c.properties.apiVersion), VK_API_VERSION_MINOR(c.properties.apiVersion),
                    VK_API_VERSION_MAJOR(NOIR_VK_API_VERSION), VK_API_VERSION_MINOR(NOIR_VK_API_VERSION));
            continue;
        }
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &familyCount, NULL);
        VkQueueFamilyProperties* families = malloc(sizeof(VkQueueFamilyProperties) * (familyCount ? familyCount : 1));
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &familyCount, families);
        for (uint32_t f = 0; f < familyCount && c.queueFamily == UINT32_MAX; f++) {
            if ((families[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presents(devices[d], f, user)) {
                c.queueFamily = f;
            }
        }
        free(families);
        if (c.queueFamily != UINT32_MAX) {
            candidates[usable++] = c;
        }
    }

    const Candidate* best = NULL;
    const char* reason = NULL;
    const char* wanted = getenv("NOIR_VK_DEVICE");
    if (wanted && *wanted) {
        for (uint32_t i = 0; i < usable && !best; i++) {
            if (names(&candidates[i], wanted)) best = &candidates[i];
        }
        if (best) reason = "NOIR_VK_DEVICE";
        else fprintf(stderr, "%s: NOIR_VK_DEVICE=%s matches no usable device; choosing one\n", who, wanted);
    }
    // A Vulkan 1.0 instance has this only through NOIR_VK_INSTANCE_EXTENSION;
    // NULL when the caller could not enable it.
    PFN_vkGetPhysicalDeviceProperties2KHR getProperties2 = (PFN_vkGetPhysicalDeviceProperties2KHR)
        vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
    dev_t node;
    if (!best && getProperties2 && x_server_device(connection, root, &node)) {
        for (uint32_t i = 0; i < usable && !best; i++) {
            if (drives_node(getProperties2, candidates[i].device, node)) best = &candidates[i];
        }
        if (best) reason = "drives the X server";
    }
    if (!best) {
        for (uint32_t i = 0; i < usable; i++) {
            if (!best || better(&candidates[i], best)) best = &candidates[i];
        }
        reason = "preferred device type";
    }

    if (best) {
        choice->device = best->device;
        choice->queueFamily = best->queueFamily;
        choice->properties = best->properties;
        choice->reason = reason;
    } else {
        fprintf(stderr, "%s: no Vulkan %u.%u device can present to this X server\n", who,
                VK_API_VERSION_MAJOR(NOIR_VK_API_VERSION), VK_API_VERSION_MINOR(NOIR_VK_API_VERSION));
    }
    free(candidates);
    free(devices);
    return best != NULL;
}
