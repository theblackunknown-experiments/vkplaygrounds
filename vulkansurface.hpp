#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <cinttypes>

#include "vulkansurfacebase.hpp"
#include "vulkansurfacemixin.hpp"
#include "vulkanphysicaldevicebase.hpp"
#include "vulkanphysicaldevicemixin.hpp"

struct VulkanSurface
    : public VulkanPhysicalDeviceBase
    , public VulkanPhysicalDeviceMixin<VulkanSurface>
    , public VulkanSurfaceBase
    , public VulkanSurfaceMixin<VulkanSurface>
{
    explicit VulkanSurface(
        const VkInstance& instance,
        const VkPhysicalDevice& physical_device,
        const VkDevice& device,
        const VkSurfaceKHR& surface,
        const VkExtent2D& resolution,
        bool vsync);
    ~VulkanSurface();

    void generate_swapchain();
    void submit(std::uint32_t idx);

    VkInstance                   mInstance;
    VkPhysicalDevice             mPhysicalDevice;
    VkDevice                     mDevice;
    VkSurfaceKHR                 mSurface;

    std::uint32_t                mQueueFamilyIndex;
    VkFormat                     mColorFormat;
    VkColorSpaceKHR              mColorSpace;
    VkPresentModeKHR             mPresentMode;
    VkCommandPool                mCommandPool;
    VkSemaphore                  mSemaphorePresentComplete = VK_NULL_HANDLE;
    VkSemaphore                  mSemaphoreRenderComplete  = VK_NULL_HANDLE;

    VkExtent2D                   mResolution;
    VkQueue                      mQueue     = VK_NULL_HANDLE;
    VkSwapchainKHR               mSwapChain = VK_NULL_HANDLE;

    struct Buffer {
        VkImage     image;
        VkImageView view;
    };

    std::uint32_t                mCount;
    std::vector<VkImage>         mImages;
    std::vector<Buffer>          mBuffers;
    std::vector<VkCommandBuffer> mCommandBuffers;
    std::vector<VkFence>         mWaitFences;
};
