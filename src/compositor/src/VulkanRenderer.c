#include "VulkanRenderer.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shaders/quad_vert.h"
#include "shaders/quad_frag.h"

#define TEXTURE_FORMAT VK_FORMAT_B8G8R8A8_UNORM   // X11 ZPixmap byte order
#define MAX_TEXTURES 1024

struct RendererTexture {
    int width, height;
    VkImage image;
    VkDeviceMemory imageMemory;
    VkImageView view;
    VkDescriptorSet descriptor;
    VkBuffer staging;
    VkDeviceMemory stagingMemory;
    uint8_t* pixels;          // staging memory, mapped for the texture's lifetime
    int initialized;          // image has left VK_IMAGE_LAYOUT_UNDEFINED
    int dirty;                // queued in renderer->dirty
    int dirtyX0, dirtyY0, dirtyX1, dirtyY1;
};

typedef struct {
    float rect[4];
    float params[4];
    float screen[2];
    float mode;
    float opacity;
} PushConstants;

struct VulkanRenderer {
    Display* display;
    Window target;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    char deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkDevice device;
    uint32_t queueFamily;
    VkQueue queue;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkSwapchainKHR swapchain;
    VkExtent2D extent;
    uint32_t imageCount;
    VkImage* images;
    VkImageView* imageViews;
    VkFramebuffer* framebuffers;
    VkRenderPass renderPass;
    VkDescriptorSetLayout descriptorLayout;
    VkDescriptorPool descriptorPool;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkSampler sampler;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailable;
    VkSemaphore renderFinished;
    VkFence inFlight;
    uint32_t imageIndex;
    RendererTexture* blank;        // bound while drawing untextured quads
    RendererTexture** dirty;
    int dirtyCount, dirtyCapacity;
};

#define CHECK(call, message) \
    do { if ((call) != VK_SUCCESS) { fprintf(stderr, "NoirCompositor: %s\n", message); return 0; } } while (0)

// --- device ------------------------------------------------------------------

static int create_instance(VulkanRenderer* r) {
    VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                              .pApplicationName = "NoirCompositor",
                              .apiVersion = VK_API_VERSION_1_0 };
    const char* extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME };
    VkInstanceCreateInfo info = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                  .pApplicationInfo = &app,
                                  .enabledExtensionCount = 2,
                                  .ppEnabledExtensionNames = extensions };
    // Validation is opt-in: the layer is a development package.
    const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
    if (getenv("NOIR_VK_VALIDATION")) {
        info.enabledLayerCount = 1;
        info.ppEnabledLayerNames = layers;
    }
    CHECK(vkCreateInstance(&info, NULL, &r->instance), "cannot create a Vulkan instance");
    return 1;
}

// First device with a queue family that can both render and present to the
// surface. NOIR_VK_DEVICE=<index> picks a specific one.
static int pick_device(VulkanRenderer* r) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(r->instance, &count, NULL);
    if (count == 0) {
        fprintf(stderr, "NoirCompositor: no Vulkan devices\n");
        return 0;
    }
    VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * count);
    vkEnumeratePhysicalDevices(r->instance, &count, devices);
    const char* wanted = getenv("NOIR_VK_DEVICE");
    int found = 0;
    for (uint32_t d = 0; d < count && !found; d++) {
        if (wanted && (uint32_t)atoi(wanted) != d) {
            continue;
        }
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &familyCount, NULL);
        VkQueueFamilyProperties* families = malloc(sizeof(VkQueueFamilyProperties) * familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &familyCount, families);
        for (uint32_t f = 0; f < familyCount && !found; f++) {
            VkBool32 canPresent = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(devices[d], f, r->surface, &canPresent);
            uint32_t formats = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(devices[d], r->surface, &formats, NULL);
            if ((families[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent && formats > 0) {
                r->physicalDevice = devices[d];
                r->queueFamily = f;
                found = 1;
            }
        }
        free(families);
    }
    free(devices);
    if (!found) {
        fprintf(stderr, "NoirCompositor: no Vulkan device can present to this X server\n");
        return 0;
    }
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(r->physicalDevice, &properties);
    snprintf(r->deviceName, sizeof(r->deviceName), "%s", properties.deviceName);
    vkGetPhysicalDeviceMemoryProperties(r->physicalDevice, &r->memoryProperties);
    return 1;
}

static int create_device(VulkanRenderer* r) {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                          .queueFamilyIndex = r->queueFamily,
                                          .queueCount = 1,
                                          .pQueuePriorities = &priority };
    const char* extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo info = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                .queueCreateInfoCount = 1,
                                .pQueueCreateInfos = &queueInfo,
                                .enabledExtensionCount = 1,
                                .ppEnabledExtensionNames = extensions };
    CHECK(vkCreateDevice(r->physicalDevice, &info, NULL, &r->device), "cannot create a Vulkan device");
    vkGetDeviceQueue(r->device, r->queueFamily, 0, &r->queue);

    VkCommandPoolCreateInfo poolInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                         .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                         .queueFamilyIndex = r->queueFamily };
    CHECK(vkCreateCommandPool(r->device, &poolInfo, NULL, &r->commandPool), "cannot create a command pool");
    VkCommandBufferAllocateInfo allocInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                              .commandPool = r->commandPool,
                                              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                              .commandBufferCount = 1 };
    CHECK(vkAllocateCommandBuffers(r->device, &allocInfo, &r->commandBuffer), "cannot allocate a command buffer");

    VkSemaphoreCreateInfo semaphoreInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                    .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    CHECK(vkCreateSemaphore(r->device, &semaphoreInfo, NULL, &r->imageAvailable), "cannot create a semaphore");
    CHECK(vkCreateSemaphore(r->device, &semaphoreInfo, NULL, &r->renderFinished), "cannot create a semaphore");
    CHECK(vkCreateFence(r->device, &fenceInfo, NULL, &r->inFlight), "cannot create a fence");
    return 1;
}

static uint32_t find_memory_type(VulkanRenderer* r, uint32_t typeBits, VkMemoryPropertyFlags flags) {
    for (uint32_t i = 0; i < r->memoryProperties.memoryTypeCount; i++) {
        if ((typeBits & (1u << i)) && (r->memoryProperties.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    return UINT32_MAX;
}

// --- swapchain ---------------------------------------------------------------

static void destroy_swapchain(VulkanRenderer* r) {
    for (uint32_t i = 0; i < r->imageCount; i++) {
        if (r->framebuffers && r->framebuffers[i]) vkDestroyFramebuffer(r->device, r->framebuffers[i], NULL);
        if (r->imageViews && r->imageViews[i]) vkDestroyImageView(r->device, r->imageViews[i], NULL);
    }
    free(r->framebuffers); r->framebuffers = NULL;
    free(r->imageViews); r->imageViews = NULL;
    free(r->images); r->images = NULL;
    r->imageCount = 0;
    if (r->swapchain) {
        vkDestroySwapchainKHR(r->device, r->swapchain, NULL);
        r->swapchain = VK_NULL_HANDLE;
    }
}

static int create_render_pass(VulkanRenderer* r) {
    VkAttachmentDescription color = { .format = r->surfaceFormat.format,
                                      .samples = VK_SAMPLE_COUNT_1_BIT,
                                      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
    VkAttachmentReference colorRef = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                     .colorAttachmentCount = 1,
                                     .pColorAttachments = &colorRef };
    VkSubpassDependency dependency = { .srcSubpass = VK_SUBPASS_EXTERNAL,
                                       .dstSubpass = 0,
                                       .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };
    VkRenderPassCreateInfo info = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                    .attachmentCount = 1, .pAttachments = &color,
                                    .subpassCount = 1, .pSubpasses = &subpass,
                                    .dependencyCount = 1, .pDependencies = &dependency };
    CHECK(vkCreateRenderPass(r->device, &info, NULL, &r->renderPass), "cannot create the render pass");
    return 1;
}

static int choose_surface_format(VulkanRenderer* r) {
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(r->physicalDevice, r->surface, &count, NULL);
    if (count == 0) {
        return 0;
    }
    VkSurfaceFormatKHR* formats = malloc(sizeof(VkSurfaceFormatKHR) * count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(r->physicalDevice, r->surface, &count, formats);
    r->surfaceFormat = formats[0];
    // Window pixels are already display-referred: a UNORM target passes them
    // through untouched, an sRGB one would re-encode them.
    for (uint32_t i = 0; i < count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            r->surfaceFormat = formats[i];
            break;
        }
    }
    free(formats);
    return 1;
}

static int create_swapchain(VulkanRenderer* r) {
    VkSurfaceCapabilitiesKHR caps;
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(r->physicalDevice, r->surface, &caps),
          "cannot query the surface");
    r->extent = caps.currentExtent;
    if (caps.currentExtent.width == UINT32_MAX) {
        XWindowAttributes wa;
        XGetWindowAttributes(r->display, r->target, &wa);
        r->extent.width = (uint32_t)wa.width;
        r->extent.height = (uint32_t)wa.height;
    }
    if (r->extent.width == 0 || r->extent.height == 0) {
        return 0;
    }
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }
    VkSwapchainCreateInfoKHR info = { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                      .surface = r->surface,
                                      .minImageCount = imageCount,
                                      .imageFormat = r->surfaceFormat.format,
                                      .imageColorSpace = r->surfaceFormat.colorSpace,
                                      .imageExtent = r->extent,
                                      .imageArrayLayers = 1,
                                      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                      .preTransform = caps.currentTransform,
                                      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                                      .clipped = VK_TRUE };
    CHECK(vkCreateSwapchainKHR(r->device, &info, NULL, &r->swapchain), "cannot create the swapchain");

    vkGetSwapchainImagesKHR(r->device, r->swapchain, &r->imageCount, NULL);
    r->images = calloc(r->imageCount, sizeof(VkImage));
    r->imageViews = calloc(r->imageCount, sizeof(VkImageView));
    r->framebuffers = calloc(r->imageCount, sizeof(VkFramebuffer));
    vkGetSwapchainImagesKHR(r->device, r->swapchain, &r->imageCount, r->images);
    for (uint32_t i = 0; i < r->imageCount; i++) {
        VkImageViewCreateInfo viewInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                           .image = r->images[i],
                                           .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                           .format = r->surfaceFormat.format,
                                           .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
        CHECK(vkCreateImageView(r->device, &viewInfo, NULL, &r->imageViews[i]), "cannot create an image view");
        VkFramebufferCreateInfo fbInfo = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                           .renderPass = r->renderPass,
                                           .attachmentCount = 1,
                                           .pAttachments = &r->imageViews[i],
                                           .width = r->extent.width,
                                           .height = r->extent.height,
                                           .layers = 1 };
        CHECK(vkCreateFramebuffer(r->device, &fbInfo, NULL, &r->framebuffers[i]), "cannot create a framebuffer");
    }
    return 1;
}

static int recreate_swapchain(VulkanRenderer* r) {
    vkDeviceWaitIdle(r->device);
    destroy_swapchain(r);
    return create_swapchain(r);
}

// --- pipeline ----------------------------------------------------------------

static VkShaderModule create_shader(VulkanRenderer* r, const uint32_t* code, size_t size) {
    VkShaderModuleCreateInfo info = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                      .codeSize = size, .pCode = code };
    VkShaderModule module = VK_NULL_HANDLE;
    vkCreateShaderModule(r->device, &info, NULL, &module);
    return module;
}

static int create_pipeline(VulkanRenderer* r) {
    VkDescriptorSetLayoutBinding binding = { .binding = 0,
                                             .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                             .descriptorCount = 1,
                                             .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT };
    VkDescriptorSetLayoutCreateInfo layoutInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                   .bindingCount = 1, .pBindings = &binding };
    CHECK(vkCreateDescriptorSetLayout(r->device, &layoutInfo, NULL, &r->descriptorLayout),
          "cannot create the descriptor layout");

    VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES };
    VkDescriptorPoolCreateInfo poolInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                            .maxSets = MAX_TEXTURES,
                                            .poolSizeCount = 1, .pPoolSizes = &poolSize };
    CHECK(vkCreateDescriptorPool(r->device, &poolInfo, NULL, &r->descriptorPool),
          "cannot create the descriptor pool");

    // Windows are drawn 1:1, so nearest sampling keeps them pixel exact.
    VkSamplerCreateInfo samplerInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                        .magFilter = VK_FILTER_NEAREST, .minFilter = VK_FILTER_NEAREST,
                                        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE };
    CHECK(vkCreateSampler(r->device, &samplerInfo, NULL, &r->sampler), "cannot create the sampler");

    VkPushConstantRange pushRange = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                      0, sizeof(PushConstants) };
    VkPipelineLayoutCreateInfo plInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                          .setLayoutCount = 1, .pSetLayouts = &r->descriptorLayout,
                                          .pushConstantRangeCount = 1, .pPushConstantRanges = &pushRange };
    CHECK(vkCreatePipelineLayout(r->device, &plInfo, NULL, &r->pipelineLayout), "cannot create the pipeline layout");

    VkShaderModule vert = create_shader(r, quad_vert_spv, sizeof(quad_vert_spv));
    VkShaderModule frag = create_shader(r, quad_frag_spv, sizeof(quad_frag_spv));
    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main" },
    };
    VkPipelineVertexInputStateCreateInfo vertexInput = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo assembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
    VkPipelineViewportStateCreateInfo viewport = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                   .viewportCount = 1, .scissorCount = 1 };
    VkPipelineRasterizationStateCreateInfo raster = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                      .polygonMode = VK_POLYGON_MODE_FILL,
                                                      .cullMode = VK_CULL_MODE_NONE,
                                                      .lineWidth = 1.0f };
    VkPipelineMultisampleStateCreateInfo multisample = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                         .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT };
    // Everything is premultiplied alpha, "over".
    VkPipelineColorBlendAttachmentState blend = { .blendEnable = VK_TRUE,
                                                  .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                  .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                  .colorBlendOp = VK_BLEND_OP_ADD,
                                                  .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                  .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                  .alphaBlendOp = VK_BLEND_OP_ADD,
                                                  .colorWriteMask = 0xf };
    VkPipelineColorBlendStateCreateInfo blending = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                     .attachmentCount = 1, .pAttachments = &blend };
    const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                 .dynamicStateCount = 2, .pDynamicStates = dynamicStates };
    VkGraphicsPipelineCreateInfo info = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                          .stageCount = 2, .pStages = stages,
                                          .pVertexInputState = &vertexInput,
                                          .pInputAssemblyState = &assembly,
                                          .pViewportState = &viewport,
                                          .pRasterizationState = &raster,
                                          .pMultisampleState = &multisample,
                                          .pColorBlendState = &blending,
                                          .pDynamicState = &dynamic,
                                          .layout = r->pipelineLayout,
                                          .renderPass = r->renderPass };
    VkResult result = vkCreateGraphicsPipelines(r->device, VK_NULL_HANDLE, 1, &info, NULL, &r->pipeline);
    vkDestroyShaderModule(r->device, vert, NULL);
    vkDestroyShaderModule(r->device, frag, NULL);
    CHECK(result, "cannot create the graphics pipeline");
    return 1;
}

// --- textures ----------------------------------------------------------------

static void dirty_remove(VulkanRenderer* r, RendererTexture* texture) {
    for (int i = 0; i < r->dirtyCount; i++) {
        if (r->dirty[i] == texture) {
            r->dirty[i] = r->dirty[--r->dirtyCount];
            return;
        }
    }
}

RendererTexture* renderer_texture_create(VulkanRenderer* r, int width, int height) {
    if (width <= 0 || height <= 0) {
        return NULL;
    }
    RendererTexture* t = calloc(1, sizeof(RendererTexture));
    t->width = width;
    t->height = height;
    const VkDeviceSize bytes = (VkDeviceSize)width * (VkDeviceSize)height * 4;

    VkImageCreateInfo imageInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                    .imageType = VK_IMAGE_TYPE_2D,
                                    .format = TEXTURE_FORMAT,
                                    .extent = { (uint32_t)width, (uint32_t)height, 1 },
                                    .mipLevels = 1, .arrayLayers = 1,
                                    .samples = VK_SAMPLE_COUNT_1_BIT,
                                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                                    .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                      .size = bytes,
                                      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT };
    VkMemoryRequirements req;
    VkMemoryAllocateInfo alloc = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };

    if (vkCreateImage(r->device, &imageInfo, NULL, &t->image) != VK_SUCCESS) goto fail;
    vkGetImageMemoryRequirements(r->device, t->image, &req);
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = find_memory_type(r, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (alloc.memoryTypeIndex == UINT32_MAX) alloc.memoryTypeIndex = find_memory_type(r, req.memoryTypeBits, 0);
    if (vkAllocateMemory(r->device, &alloc, NULL, &t->imageMemory) != VK_SUCCESS) goto fail;
    vkBindImageMemory(r->device, t->image, t->imageMemory, 0);

    VkImageViewCreateInfo viewInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                       .image = t->image,
                                       .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                       .format = TEXTURE_FORMAT,
                                       .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
    if (vkCreateImageView(r->device, &viewInfo, NULL, &t->view) != VK_SUCCESS) goto fail;

    if (vkCreateBuffer(r->device, &bufferInfo, NULL, &t->staging) != VK_SUCCESS) goto fail;
    vkGetBufferMemoryRequirements(r->device, t->staging, &req);
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = find_memory_type(r, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (alloc.memoryTypeIndex == UINT32_MAX) goto fail;
    if (vkAllocateMemory(r->device, &alloc, NULL, &t->stagingMemory) != VK_SUCCESS) goto fail;
    vkBindBufferMemory(r->device, t->staging, t->stagingMemory, 0);
    void* mapped = NULL;
    if (vkMapMemory(r->device, t->stagingMemory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS) goto fail;
    t->pixels = mapped;
    memset(t->pixels, 0, (size_t)bytes);

    VkDescriptorSetAllocateInfo setInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                            .descriptorPool = r->descriptorPool,
                                            .descriptorSetCount = 1,
                                            .pSetLayouts = &r->descriptorLayout };
    if (vkAllocateDescriptorSets(r->device, &setInfo, &t->descriptor) != VK_SUCCESS) goto fail;
    VkDescriptorImageInfo descriptorImage = { r->sampler, t->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                   .dstSet = t->descriptor, .dstBinding = 0,
                                   .descriptorCount = 1,
                                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   .pImageInfo = &descriptorImage };
    vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);

    // A sampled image must hold defined contents: upload the cleared store.
    renderer_texture_update(r, t, 0, 0, width, height);
    return t;

fail:
    fprintf(stderr, "NoirCompositor: cannot create a %dx%d texture\n", width, height);
    renderer_texture_destroy(r, t);
    return NULL;
}

void renderer_texture_destroy(VulkanRenderer* r, RendererTexture* t) {
    if (!t) {
        return;
    }
    // The last submitted frame may still sample this texture.
    vkWaitForFences(r->device, 1, &r->inFlight, VK_TRUE, UINT64_MAX);
    dirty_remove(r, t);
    if (t->descriptor) vkFreeDescriptorSets(r->device, r->descriptorPool, 1, &t->descriptor);
    if (t->view) vkDestroyImageView(r->device, t->view, NULL);
    if (t->image) vkDestroyImage(r->device, t->image, NULL);
    if (t->imageMemory) vkFreeMemory(r->device, t->imageMemory, NULL);
    if (t->staging) vkDestroyBuffer(r->device, t->staging, NULL);
    if (t->stagingMemory) vkFreeMemory(r->device, t->stagingMemory, NULL);
    free(t);
}

uint8_t* renderer_texture_pixels(RendererTexture* t) {
    return t->pixels;
}

void renderer_texture_update(VulkanRenderer* r, RendererTexture* t, int x, int y, int width, int height) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + width > t->width ? t->width : x + width;
    int y1 = y + height > t->height ? t->height : y + height;
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    if (t->dirty) {
        if (x0 < t->dirtyX0) t->dirtyX0 = x0;
        if (y0 < t->dirtyY0) t->dirtyY0 = y0;
        if (x1 > t->dirtyX1) t->dirtyX1 = x1;
        if (y1 > t->dirtyY1) t->dirtyY1 = y1;
        return;
    }
    if (r->dirtyCount == r->dirtyCapacity) {
        r->dirtyCapacity = r->dirtyCapacity ? r->dirtyCapacity * 2 : 32;
        r->dirty = realloc(r->dirty, sizeof(RendererTexture*) * (size_t)r->dirtyCapacity);
    }
    r->dirty[r->dirtyCount++] = t;
    t->dirty = 1;
    t->dirtyX0 = x0; t->dirtyY0 = y0; t->dirtyX1 = x1; t->dirtyY1 = y1;
}

static void image_barrier(VkCommandBuffer cb, VkImage image, VkImageLayout from, VkImageLayout to,
                          VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                          VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
    VkImageMemoryBarrier barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                     .srcAccessMask = srcAccess, .dstAccessMask = dstAccess,
                                     .oldLayout = from, .newLayout = to,
                                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .image = image,
                                     .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
    vkCmdPipelineBarrier(cb, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

static void upload_dirty_textures(VulkanRenderer* r) {
    for (int i = 0; i < r->dirtyCount; i++) {
        RendererTexture* t = r->dirty[i];
        image_barrier(r->commandBuffer, t->image,
                      t->initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      t->initialized ? VK_ACCESS_SHADER_READ_BIT : 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                      t->initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      VK_PIPELINE_STAGE_TRANSFER_BIT);
        // An image coming from UNDEFINED has no contents to keep: send it all.
        const int x0 = t->initialized ? t->dirtyX0 : 0, y0 = t->initialized ? t->dirtyY0 : 0;
        const int x1 = t->initialized ? t->dirtyX1 : t->width, y1 = t->initialized ? t->dirtyY1 : t->height;
        VkBufferImageCopy region = { .bufferOffset = ((VkDeviceSize)y0 * (VkDeviceSize)t->width + (VkDeviceSize)x0) * 4,
                                     .bufferRowLength = (uint32_t)t->width,
                                     .bufferImageHeight = (uint32_t)t->height,
                                     .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                                     .imageOffset = { x0, y0, 0 },
                                     .imageExtent = { (uint32_t)(x1 - x0), (uint32_t)(y1 - y0), 1 } };
        vkCmdCopyBufferToImage(r->commandBuffer, t->staging, t->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        image_barrier(r->commandBuffer, t->image,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        t->initialized = 1;
        t->dirty = 0;
    }
    r->dirtyCount = 0;
}

// --- frames ------------------------------------------------------------------

int renderer_begin_frame(VulkanRenderer* r) {
    // One frame in flight: after this wait the staging buffers are ours again.
    vkWaitForFences(r->device, 1, &r->inFlight, VK_TRUE, UINT64_MAX);

    VkResult acquired = vkAcquireNextImageKHR(r->device, r->swapchain, UINT64_MAX,
                                              r->imageAvailable, VK_NULL_HANDLE, &r->imageIndex);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain(r);
        return 0;
    }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
        return 0;
    }
    vkResetFences(r->device, 1, &r->inFlight);

    VkCommandBuffer cb = r->commandBuffer;
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                           .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cb, &beginInfo);
    upload_dirty_textures(r);

    VkClearValue clear = { .color = { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    VkRenderPassBeginInfo passInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                       .renderPass = r->renderPass,
                                       .framebuffer = r->framebuffers[r->imageIndex],
                                       .renderArea = { { 0, 0 }, r->extent },
                                       .clearValueCount = 1, .pClearValues = &clear };
    vkCmdBeginRenderPass(cb, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, r->pipeline);
    VkViewport viewport = { 0.0f, 0.0f, (float)r->extent.width, (float)r->extent.height, 0.0f, 1.0f };
    VkRect2D scissor = { { 0, 0 }, r->extent };
    vkCmdSetViewport(cb, 0, 1, &viewport);
    vkCmdSetScissor(cb, 0, 1, &scissor);
    return 1;
}

static void draw_quad(VulkanRenderer* r, RendererTexture* texture, const PushConstants* push) {
    vkCmdBindDescriptorSets(r->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, r->pipelineLayout,
                            0, 1, &texture->descriptor, 0, NULL);
    vkCmdPushConstants(r->commandBuffer, r->pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(*push), push);
    vkCmdDraw(r->commandBuffer, 6, 1, 0, 0);
}

void renderer_draw_texture(VulkanRenderer* r, RendererTexture* texture, int x, int y, int width, int height,
                           int hasAlpha, float opacity) {
    // Textures created after begin_frame have not been uploaded yet.
    if (!texture || !texture->initialized) {
        return;
    }
    PushConstants push = { .rect = { (float)x, (float)y, (float)width, (float)height },
                           .screen = { (float)r->extent.width, (float)r->extent.height },
                           .mode = hasAlpha ? 1.0f : 0.0f,
                           .opacity = opacity };
    draw_quad(r, texture, &push);
}

void renderer_draw_shadow(VulkanRenderer* r, int x, int y, int width, int height, int radius, float strength) {
    if (!r->blank || !r->blank->initialized || radius <= 0) {
        return;
    }
    PushConstants push = { .rect = { (float)(x - radius), (float)(y - radius),
                                     (float)(width + 2 * radius), (float)(height + 2 * radius) },
                           .params = { (float)radius, (float)radius, (float)width, (float)height },
                           .screen = { (float)r->extent.width, (float)r->extent.height },
                           .mode = 2.0f,
                           .opacity = strength };
    draw_quad(r, r->blank, &push);
}

void renderer_end_frame(VulkanRenderer* r) {
    vkCmdEndRenderPass(r->commandBuffer);
    vkEndCommandBuffer(r->commandBuffer);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .waitSemaphoreCount = 1, .pWaitSemaphores = &r->imageAvailable,
                            .pWaitDstStageMask = &waitStage,
                            .commandBufferCount = 1, .pCommandBuffers = &r->commandBuffer,
                            .signalSemaphoreCount = 1, .pSignalSemaphores = &r->renderFinished };
    if (vkQueueSubmit(r->queue, 1, &submit, r->inFlight) != VK_SUCCESS) {
        fprintf(stderr, "NoirCompositor: failed to submit a frame\n");
        return;
    }
    VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                 .waitSemaphoreCount = 1, .pWaitSemaphores = &r->renderFinished,
                                 .swapchainCount = 1, .pSwapchains = &r->swapchain,
                                 .pImageIndices = &r->imageIndex };
    VkResult presented = vkQueuePresentKHR(r->queue, &present);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
        recreate_swapchain(r);
    }
}

// --- lifetime ----------------------------------------------------------------

const char* renderer_device_name(const VulkanRenderer* r) {
    return r->deviceName;
}

static int renderer_init(VulkanRenderer* r) {
    if (!create_instance(r)) return 0;
    VkXlibSurfaceCreateInfoKHR surfaceInfo = { .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                                               .dpy = r->display, .window = r->target };
    CHECK(vkCreateXlibSurfaceKHR(r->instance, &surfaceInfo, NULL, &r->surface), "cannot create a Vulkan surface");
    if (!pick_device(r) || !create_device(r) || !choose_surface_format(r)) return 0;
    if (!create_render_pass(r) || !create_swapchain(r) || !create_pipeline(r)) return 0;
    r->blank = renderer_texture_create(r, 1, 1);
    return r->blank != NULL;
}

VulkanRenderer* renderer_create(Display* display, Window target) {
    VulkanRenderer* r = calloc(1, sizeof(VulkanRenderer));
    if (!r) {
        return NULL;
    }
    r->display = display;
    r->target = target;
    if (!renderer_init(r)) {
        renderer_destroy(r);
        return NULL;
    }
    return r;
}

void renderer_destroy(VulkanRenderer* r) {
    if (!r) {
        return;
    }
    if (r->device) {
        vkDeviceWaitIdle(r->device);
        if (r->blank) renderer_texture_destroy(r, r->blank);
        destroy_swapchain(r);
        if (r->pipeline) vkDestroyPipeline(r->device, r->pipeline, NULL);
        if (r->pipelineLayout) vkDestroyPipelineLayout(r->device, r->pipelineLayout, NULL);
        if (r->sampler) vkDestroySampler(r->device, r->sampler, NULL);
        if (r->descriptorPool) vkDestroyDescriptorPool(r->device, r->descriptorPool, NULL);
        if (r->descriptorLayout) vkDestroyDescriptorSetLayout(r->device, r->descriptorLayout, NULL);
        if (r->renderPass) vkDestroyRenderPass(r->device, r->renderPass, NULL);
        if (r->imageAvailable) vkDestroySemaphore(r->device, r->imageAvailable, NULL);
        if (r->renderFinished) vkDestroySemaphore(r->device, r->renderFinished, NULL);
        if (r->inFlight) vkDestroyFence(r->device, r->inFlight, NULL);
        if (r->commandPool) vkDestroyCommandPool(r->device, r->commandPool, NULL);
        vkDestroyDevice(r->device, NULL);
    }
    if (r->instance) {
        if (r->surface) vkDestroySurfaceKHR(r->instance, r->surface, NULL);
        vkDestroyInstance(r->instance, NULL);
    }
    free(r->dirty);
    free(r);
}
