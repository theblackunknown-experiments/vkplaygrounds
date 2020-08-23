#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>

#include <span>
#include <tuple>
#include <vector>

struct VulkanPresentation
{
    explicit VulkanPresentation(
        VkPhysicalDevice vkphysicaldevice,
        VkDevice vkdevice,
        std::uint32_t queue_family_index,
        const std::span<VkQueue>& vkqueues,
        VkSurfaceKHR vksurface,
        VkExtent2D resolution,
        bool vsync
    );
    ~VulkanPresentation();

    static
    bool supports(VkPhysicalDevice vkphysicaldevice, VkSurfaceKHR vksurface);

    static
    std::tuple<std::uint32_t, std::uint32_t> requirements(VkPhysicalDevice vkphysicaldevice, VkSurfaceKHR vksurface);

    // Setup

    void recreate_swapchain();

    void submit(std::uint32_t idx);

    // States

    VkPhysicalDevice                     mPhysicalDevice  = VK_NULL_HANDLE;
    std::uint32_t                        mQueueFamilyIndex;
    VkDevice                             mDevice          = VK_NULL_HANDLE;
    VkQueue                              mQueue           = VK_NULL_HANDLE;

    VkPhysicalDeviceFeatures             mFeatures;
    VkPhysicalDeviceProperties           mProperties;
    VkPhysicalDeviceMemoryProperties     mMemoryProperties;

    VkSurfaceKHR                         mSurface;
    VkExtent2D                           mResolution;
    VkFormat                             mColorFormat;
    VkColorSpaceKHR                      mColorSpace;
    VkPresentModeKHR                     mPresentMode;

    VkCommandPool                        mCommandPool = VK_NULL_HANDLE;

    VkSwapchainKHR                       mSwapChain   = VK_NULL_HANDLE;

    std::uint32_t                        mCount;
    std::vector<VkImage>                 mImages;
    std::vector<VkImageView>             mImageViews;
    std::vector<VkCommandBuffer>         mCommandBuffers;
    std::vector<VkFence>                 mWaitFences;

    // NOTE To scale, we would need to have an array of semaphore present/complete if we want to process frame as fast as possible
    VkSemaphore                          mSemaphorePresentComplete = VK_NULL_HANDLE;
    VkSemaphore                          mSemaphoreRenderComplete  = VK_NULL_HANDLE;

};
