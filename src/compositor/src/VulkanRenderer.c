#include "VulkanRenderer.h"
#include <stdlib.h>
#include <stdio.h>

VkResult createVulkanInstance(VulkanRenderer* renderer) {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "SiriusOS Vulkan Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    const char* extensions[] = {
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME

    };
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extensions;
    const char* validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = validationLayers;

    return vkCreateInstance(&createInfo, NULL, &renderer->instance);
}

VulkanQueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    VulkanQueueFamilyIndices indices = {0};
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
    VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
            indices.graphicsFamilyHasValue = 1;
        }
        VkBool32 presentSupport = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
            indices.presentFamilyHasValue = 1;
        }
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
            indices.computeFamilyHasValue = 1;
        }
        if (indices.graphicsFamilyHasValue && indices.presentFamilyHasValue && indices.computeFamilyHasValue) {
            break;
        }
    }
    free(queueFamilies);
    return indices;
}


VkResult initializeVulkanDevice(VulkanRenderer* renderer) {
    int deviceCount = 0;

    vkEnumeratePhysicalDevices(renderer->instance, &deviceCount, NULL);
    if (deviceCount == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * deviceCount);
    vkEnumeratePhysicalDevices(renderer->instance, &deviceCount, devices);
    renderer->physicalDevice = devices[0];
    free(devices);
    
    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
   
    VkDeviceQueueCreateInfo* queueCreateInfos;
    uint32_t queueCreateInfoCount = 0;
    VulkanQueueFamilyIndices indices = findQueueFamilies(renderer->physicalDevice, renderer->surface);
    renderer->graphicsQueueFamilyIndex = indices.graphicsFamily;
    renderer->presentQueueFamilyIndex = indices.presentFamily;
    renderer->computeQueueFamilyIndex = indices.computeFamily;
    if (indices.graphicsFamilyHasValue) {
        queueCreateInfoCount++;
    }
    if (indices.presentFamilyHasValue && indices.presentFamily != indices.graphicsFamily) {
        queueCreateInfoCount++;
    }
    if (indices.computeFamilyHasValue && indices.computeFamily != indices.graphicsFamily && indices.computeFamily != indices.presentFamily) {
        queueCreateInfoCount++;
    }
    queueCreateInfos = (VkDeviceQueueCreateInfo*)malloc(sizeof(VkDeviceQueueCreateInfo) * queueCreateInfoCount);
    float queuePriority = 1.0f;
    uint32_t infoIndex = 0;
    if (indices.graphicsFamilyHasValue) {
        VkDeviceQueueCreateInfo graphicsQueueCreateInfo = {};
        graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        graphicsQueueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
        graphicsQueueCreateInfo.queueCount = 1;
        graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos[infoIndex++] = graphicsQueueCreateInfo;
    }
    if (indices.presentFamilyHasValue && indices.presentFamily != indices.graphicsFamily) {
        VkDeviceQueueCreateInfo presentQueueCreateInfo = {};
        presentQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        presentQueueCreateInfo.queueFamilyIndex = indices.presentFamily;
        presentQueueCreateInfo.queueCount = 1;
        presentQueueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos[infoIndex++] = presentQueueCreateInfo;
    }
    if (indices.computeFamilyHasValue && indices.computeFamily != indices.graphicsFamily && indices.computeFamily != indices.presentFamily) {
        VkDeviceQueueCreateInfo computeQueueCreateInfo = {};
        computeQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        computeQueueCreateInfo.queueFamilyIndex = indices.computeFamily;
        computeQueueCreateInfo.queueCount = 1;
        computeQueueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos[infoIndex++] = computeQueueCreateInfo;
    }


    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = queueCreateInfoCount;
    createInfo.pQueueCreateInfos = queueCreateInfos;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = NULL;
    if (vkCreateDevice(renderer->physicalDevice, &createInfo, NULL, &renderer->device) != VK_SUCCESS) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Create synchronization primitives (fence and semaphores)
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(renderer->device, &fenceInfo, NULL, &renderer->inFlightFence) != VK_SUCCESS) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(renderer->device, &semaphoreInfo, NULL, &renderer->imageAvailableSemaphore) != VK_SUCCESS) {
        vkDestroyFence(renderer->device, renderer->inFlightFence, NULL);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (vkCreateSemaphore(renderer->device, &semaphoreInfo, NULL, &renderer->renderFinishedSemaphore) != VK_SUCCESS) {
        vkDestroySemaphore(renderer->device, renderer->imageAvailableSemaphore, NULL);
        vkDestroyFence(renderer->device, renderer->inFlightFence, NULL);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    vkGetDeviceQueue(renderer->device, renderer->graphicsQueueFamilyIndex, 0, &renderer->graphicsQueue);
    vkGetDeviceQueue(renderer->device, renderer->presentQueueFamilyIndex, 0, &renderer->presentQueue);
    vkGetDeviceQueue(renderer->device, renderer->computeQueueFamilyIndex, 0, &renderer->computeQueue);
    free(queueCreateInfos);
    return VK_SUCCESS;

}

VkResult createFramebuffers(VulkanRenderer* renderer) {
    renderer->framebuffers = malloc(sizeof(VkFramebuffer) * renderer->swapchainImageCount);
    if (!renderer->framebuffers) return VK_ERROR_OUT_OF_HOST_MEMORY;

    for (uint32_t i = 0; i < renderer->swapchainImageCount; i++) {
        VkImageView attachments[] = { renderer->swapchainImageViews[i] };

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderer->renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = renderer->swapchainExtent.width;
        fbInfo.height = renderer->swapchainExtent.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(renderer->device, &fbInfo, NULL, &renderer->framebuffers[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create framebuffer %u\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    return VK_SUCCESS;
}

VkResult initializeSwapchain(VulkanRenderer* renderer, int width, int height) {
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physicalDevice, renderer->surface, &formatCount, NULL);
    if (formatCount == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physicalDevice, renderer->surface, &formatCount, formats);
    renderer->surfaceFormat = formats[0];
    free(formats);
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physicalDevice, renderer->surface, &presentModeCount, NULL);
    if (presentModeCount == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;
        
    }
    VkPresentModeKHR* presentModes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR) * presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physicalDevice, renderer->surface, &presentModeCount, presentModes);
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // USE ONLY FIFO FOR NOW
    // for (uint32_t i = 0; i < presentModeCount; i++) {
    //     if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
    //         presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    //         fprintf(stdout, "Using MAILBOX present mode\n");
    //         break;
    //     }
    //     else if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
    //         presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    //         fprintf(stdout, "Using IMMEDIATE present mode\n");
    //     }
    // }
    free(presentModes);
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->physicalDevice, renderer->surface, &capabilities);
    renderer->swapchainExtent = capabilities.currentExtent;
    if (capabilities.currentExtent.width == (uint32_t)-1) {
        renderer->swapchainExtent.width = width;
        renderer->swapchainExtent.height = height;
    }
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = renderer->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = renderer->surfaceFormat.format;
    createInfo.imageColorSpace = renderer->surfaceFormat.colorSpace;
    createInfo.imageExtent = renderer->swapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    uint32_t queueFamilyIndices[] = { renderer->graphicsQueueFamilyIndex, renderer->presentQueueFamilyIndex };
    if (renderer->graphicsQueueFamilyIndex != renderer->presentQueueFamilyIndex) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = NULL;
    }
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(renderer->device, &createInfo, NULL, &renderer->swapchain) != VK_SUCCESS) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain, &imageCount, NULL);
    renderer->swapchainImageCount = imageCount;
    renderer->swapchainImages = (VkImage*)malloc(sizeof(VkImage) * imageCount);
    vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain, &imageCount, renderer->swapchainImages);
    renderer->swapchainImageViews = malloc(sizeof(VkImageView) * imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = renderer->swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = renderer->surfaceFormat.format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(renderer->device, &viewInfo, NULL, &renderer->swapchainImageViews[i]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create swapchain image view %u\n", i);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    return VK_SUCCESS;
}

VkResult createCommandBuffers(VulkanRenderer* renderer) {
    renderer->commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * renderer->swapchainImageCount);
    if (!renderer->commandBuffers) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    // Allocate command buffers
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = renderer->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = renderer->swapchainImageCount;
    if (vkAllocateCommandBuffers(renderer->device, &allocInfo, renderer->commandBuffers) != VK_SUCCESS) {
        free(renderer->commandBuffers);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}


VkResult createSimpleRenderPass(VulkanRenderer* renderer) {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = renderer->surfaceFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    // Basic external->subpass dependency so the image is ready for color output
    VkSubpassDependency dep = {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAttachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;

    return vkCreateRenderPass(renderer->device, &rpInfo, NULL, &renderer->renderPass);
}

VkResult createDummyPipelineLayout(VulkanRenderer* renderer) {
    VkPipelineLayoutCreateInfo plInfo = {};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount = 0;
    plInfo.pushConstantRangeCount = 0;
    return vkCreatePipelineLayout(renderer->device, &plInfo, NULL, &renderer->pipelineLayout);
}


void cleanupVulkan(VulkanRenderer* renderer) {
    vkDeviceWaitIdle(renderer->device);
    

    // Destroy framebuffers
    if (renderer->framebuffers) {
        for (uint32_t i = 0; i < renderer->swapchainImageCount; i++) {
            if (renderer->framebuffers[i]) {
                vkDestroyFramebuffer(renderer->device, renderer->framebuffers[i], NULL);
            }
        }
        free(renderer->framebuffers);
        renderer->framebuffers = NULL;
    }

    // Destroy swapchain image views
    if (renderer->swapchainImageViews) {
        for (uint32_t i = 0; i < renderer->swapchainImageCount; i++) {
            if (renderer->swapchainImageViews[i]) {
                vkDestroyImageView(renderer->device, renderer->swapchainImageViews[i], NULL);
            }
        }
        free(renderer->swapchainImageViews);
        renderer->swapchainImageViews = NULL;
    }

    // Free swapchain images array
    if (renderer->swapchainImages) {
        free(renderer->swapchainImages);
        renderer->swapchainImages = NULL;
    }

    // Free command buffers array
    if (renderer->commandBuffers) {
        free(renderer->commandBuffers);
        renderer->commandBuffers = NULL;
    }

    vkDestroyPipeline(renderer->device, renderer->graphicsPipeline, NULL);
    vkDestroyPipelineLayout(renderer->device, renderer->pipelineLayout, NULL);
    vkDestroyRenderPass(renderer->device, renderer->renderPass, NULL);
    vkDestroySemaphore(renderer->device, renderer->renderFinishedSemaphore, NULL);
    vkDestroySemaphore(renderer->device, renderer->imageAvailableSemaphore, NULL);
    vkDestroyFence(renderer->device, renderer->inFlightFence, NULL);
    vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
    vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
    vkDestroyCommandPool(renderer->device, renderer->commandPool, NULL);
    vkDestroyDevice(renderer->device, NULL);
    vkDestroyInstance(renderer->instance, NULL);
}

VkResult createVulkanSurface(VulkanRenderer* renderer, Display* display, Window window) {
    VkXlibSurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = display,
        .window = window
    };
    if (vkCreateXlibSurfaceKHR(renderer->instance, &surfaceCreateInfo, NULL, &renderer->surface) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan surface\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}


void initializeCommandPool(VulkanRenderer* renderer) {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = renderer->graphicsQueueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(renderer->device, &poolInfo, NULL, &renderer->commandPool) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create command pool\n");
    }
    if(renderer->graphicsQueueFamilyIndex != renderer->presentQueueFamilyIndex) {
        VkCommandPoolCreateInfo presentPoolInfo = {};
        presentPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        presentPoolInfo.queueFamilyIndex = renderer->presentQueueFamilyIndex;
        presentPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(renderer->device, &presentPoolInfo, NULL, &renderer->commandPool) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create present command pool\n");
        }
    }
    if (renderer->graphicsQueueFamilyIndex != renderer->computeQueueFamilyIndex && renderer->presentQueueFamilyIndex != renderer->computeQueueFamilyIndex) {
        VkCommandPoolCreateInfo computePoolInfo = {};
        computePoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        computePoolInfo.queueFamilyIndex = renderer->computeQueueFamilyIndex;
        computePoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(renderer->device, &computePoolInfo, NULL, &renderer->commandPool) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create compute command pool\n");
        }
    }
}

VulkanRenderer* createVulkanRenderer(Display* display, Window window) {
    VulkanRenderer* renderer = (VulkanRenderer*)malloc(sizeof(VulkanRenderer));
    if (!renderer) return NULL;

    if (createVulkanInstance(renderer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance\n");
        free(renderer);
        return NULL;
    }
    fprintf(stdout, "Vulkan instance created successfully\n");
    if(createVulkanSurface(renderer, display, window) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan surface\n");
        vkDestroyInstance(renderer->instance, NULL);
        free(renderer);
        return NULL;
    }
    fprintf(stdout, "Vulkan surface created successfully\n");
    if (initializeVulkanDevice(renderer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to initialize Vulkan device\n");
        vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
        vkDestroyInstance(renderer->instance, NULL);
        free(renderer);
        return NULL;
    }
    
    // Get window width and height using XGetGeometry
    Window root_return;
    int x_return, y_return;
    unsigned int width, height, border_width, depth;
    XGetGeometry(display, window, &root_return, &x_return, &y_return, &width, &height, &border_width, &depth);

    fprintf(stdout, "Vulkan device initialized successfully\n");
    if (initializeSwapchain(renderer, width, height) != VK_SUCCESS) {
        fprintf(stderr, "Failed to initialize swapchain\n");
        vkDestroyDevice(renderer->device, NULL);
        vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
        vkDestroyInstance(renderer->instance, NULL);
        free(renderer);
        return NULL;
    }
    if(createSimpleRenderPass(renderer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create render pass\n");
        vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
        vkDestroyDevice(renderer->device, NULL);
        vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
        vkDestroyInstance(renderer->instance, NULL);
        free(renderer);
        return NULL;
    }
    fprintf(stdout, "Render pass created successfully\n");
    if (createDummyPipelineLayout(renderer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create pipeline layout\n");
        vkDestroyRenderPass(renderer->device, renderer->renderPass, NULL);
        vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
        vkDestroyDevice(renderer->device, NULL);
        vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
        vkDestroyInstance(renderer->instance, NULL);
        free(renderer);
        return NULL;
    }
    fprintf(stdout, "Pipeline layout created successfully\n");

    if (createFramebuffers(renderer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create framebuffers\n");
        vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
        vkDestroyDevice(renderer->device, NULL);
        vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
        vkDestroyInstance(renderer->instance, NULL);
        free(renderer);
        return NULL;
    }

    fprintf(stdout, "Swapchain initialized successfully\n");
    initializeCommandPool(renderer);
    if (createCommandBuffers(renderer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create command buffers\n");
        vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
        vkDestroyDevice(renderer->device, NULL);
        vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
        vkDestroyInstance(renderer->instance, NULL);
        free(renderer);
        return NULL;
    }
    fprintf(stdout, "Command buffers created successfully\n");

    return renderer;
}
VulkanRenderer* createTestVulkanRenderer(int width, int height) {
    


    
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Failed to open X display\n");
        return NULL;
    }
    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    XSetWindowAttributes swa;
    swa.event_mask = ExposureMask | KeyPressMask;
    Window window = XCreateWindow(
        display, root,
        0, 0, width, height, 0,
        CopyFromParent, InputOutput,
        CopyFromParent, CWEventMask, &swa
    );
    XMapWindow(display, window);
    return createVulkanRenderer(display, window);
}
void destroyVulkanRenderer(VulkanRenderer* renderer) {
    if (renderer) {
        cleanupVulkan(renderer);

        // Clean up Vulkan resources
        free(renderer);
    }
}
void drawVulkanRendererFrame(VulkanRenderer* renderer) {
    fprintf(stdout, "Drawing frame %llu\n", renderer->currentFrame);
    renderer->currentFrame++;
    if(renderer->currentFrame >= renderer->swapchainImageCount) {
        renderer->currentFrame = 0;
    }
    // Wait for the fence to ensure the previous frame is finished
    vkWaitForFences(renderer->device, 1, &renderer->inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(renderer->device, 1, &renderer->inFlightFence);
    uint32_t imageIndex;
    vkAcquireNextImageKHR(renderer->device, renderer->swapchain, UINT64_MAX, renderer->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    // Record command buffer
    VkCommandBuffer commandBuffer = renderer->commandBuffers[imageIndex];
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        fprintf(stderr, "Failed to begin recording command buffer\n");
        return;        
    }
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderer->renderPass;
    renderPassInfo.framebuffer = renderer->framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = (VkOffset2D){ 0, 0 };
    renderPassInfo.renderArea.extent = renderer->swapchainExtent;
    VkClearValue clearColor = { .color = { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkClearAttachment clearAttach = {
    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .colorAttachment = 0,
    .clearValue.color = {{0.2f, 0.2f, 0.2f, 1.0f}} // gray border
    };

    VkClearRect rect = { 0 };
    rect.baseArrayLayer = 0;
    rect.layerCount = 1;

    // Top border (titlebar)
    rect.rect.offset = (VkOffset2D){ 0, 0 };
    rect.rect.extent = (VkExtent2D){ renderer->swapchainExtent.width, 40 };
    vkCmdClearAttachments(commandBuffer, 1, &clearAttach, 1, &rect);

    // Left border
    rect.rect.offset = (VkOffset2D){ 0, 0 };
    rect.rect.extent = (VkExtent2D){ 5, renderer->swapchainExtent.height };
    vkCmdClearAttachments(commandBuffer, 1, &clearAttach, 1, &rect);

    // Right border
    rect.rect.offset = (VkOffset2D){ renderer->swapchainExtent.width - 5, 0 };
    rect.rect.extent = (VkExtent2D){ 5, renderer->swapchainExtent.height };
    vkCmdClearAttachments(commandBuffer, 1, &clearAttach, 1, &rect);

    // Bottom border
    rect.rect.offset = (VkOffset2D){ 0, renderer->swapchainExtent.height - 5 };
    rect.rect.extent = (VkExtent2D){ renderer->swapchainExtent.width, 5 };
    vkCmdClearAttachments(commandBuffer, 1, &clearAttach, 1, &rect);

    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        fprintf(stderr, "Failed to record command buffer\n");
        return;
    }
    VkSemaphore waitSemaphores[] = { renderer->imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { renderer->renderFinishedSemaphore };
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    if (vkQueueSubmit(renderer->graphicsQueue, 1, &submitInfo, renderer->inFlightFence) != VK_SUCCESS) {
        fprintf(stderr, "Failed to submit draw command buffer\n");
        return;
    }
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapchains[] = { renderer->swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = NULL;
    vkQueuePresentKHR(renderer->presentQueue, &presentInfo);
}
