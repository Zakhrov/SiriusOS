#pragma once
#include <X11/Xlib.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>


typedef struct {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    uint32_t graphicsQueueFamilyIndex;
    uint32_t presentQueueFamilyIndex;
    uint32_t computeQueueFamilyIndex;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkCommandPool commandPool;
    VkCommandBuffer* commandBuffers;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D swapchainExtent;
    uint32_t swapchainImageCount;
    VkImage* swapchainImages;
    VkImageView* swapchainImageViews;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkFramebuffer* framebuffers;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
    uint64_t currentFrame;


    // Add more Vulkan objects as needed
} VulkanRenderer;

typedef struct {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    uint32_t computeFamily;
    int graphicsFamilyHasValue;
    int presentFamilyHasValue;
    int computeFamilyHasValue;
} VulkanQueueFamilyIndices;

VulkanQueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
VulkanRenderer* createVulkanRenderer(Display* display, Window window);
VulkanRenderer* createTestVulkanRenderer(int width, int height);
VkResult createVulkanSurface(VulkanRenderer* renderer, Display* display, Window window);
void destroyVulkanRenderer(VulkanRenderer* renderer);
void drawVulkanRendererFrame(VulkanRenderer* renderer);