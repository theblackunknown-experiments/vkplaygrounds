#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>

#include <tuple>

struct VulkanPresentation
{
    explicit VulkanPresentation(
        VkPhysicalDevice vkphysicaldevice,
        std::uint32_t queue_family_index,
        std::uint32_t queue_count,
        VkSurfaceKHR vksurface,
        VkExtent2D resolution,
        bool vsync
    );
    ~VulkanPresentation();

    static
    std::tuple<VkPhysicalDevice, std::uint32_t, std::uint32_t> requirements(const std::span<VkPhysicalDevice>& vkphysicaldevices, VkSurfaceKHR vksurface);

    // Setup

    void recreate_swapchain();

    void submit(std::uint32_t idx);

    // States

    VkPhysicalDevice                     mPhysicalDevice  = VK_NULL_HANDLE;

    VkPhysicalDeviceFeatures             mFeatures;
    VkPhysicalDeviceProperties           mProperties;
    VkPhysicalDeviceMemoryProperties     mMemoryProperties;
    std::vector<VkQueueFamilyProperties> mQueueFamiliesProperties;
    std::vector<VkExtensionProperties>   mExtensions;

    std::uint32_t                        mQueueFamilyIndex;
    VkDevice                             mDevice          = VK_NULL_HANDLE;

    VkSurfaceKHR                         mSurface;
    VkExtent2D                           mResolution;
    VkFormat                             mColorFormat;
    VkColorSpaceKHR                      mColorSpace;
    VkPresentModeKHR                     mPresentMode;

    VkQueue                              mQueue       = VK_NULL_HANDLE;
    VkCommandPool                        mCommandPool = VK_NULL_HANDLE;

    VkSwapchainKHR                       mSwapChain   = VK_NULL_HANDLE;

    std::uint32_t                mCount;
    std::vector<VkImage>         mImages;
    std::vector<VkImageView>     mImageViews;
    std::vector<VkCommandBuffer> mCommandBuffers;
    std::vector<VkFence>         mWaitFences;

    // NOTE To scale, we would need to have an array of semaphore present/complete if we want to process frame as fast as possible
    VkSemaphore                          mSemaphorePresentComplete = VK_NULL_HANDLE;
    VkSemaphore                          mSemaphoreRenderComplete  = VK_NULL_HANDLE;

};
