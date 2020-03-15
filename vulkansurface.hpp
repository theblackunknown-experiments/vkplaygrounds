#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <cinttypes>

#include "vulkansurfacebase.hpp"
#include "vulkansurfacemixin.hpp"

struct VulkanSurface
    : public VulkanSurfaceBase
    , public VulkanSurfaceMixin<VulkanSurface>
{
    explicit VulkanSurface(
        const VkInstance& instance,
        const VkPhysicalDevice& physical_device,
        const VkDevice& device,
        const VkSurfaceKHR& surface);
    ~VulkanSurface();

    VkExtent2D generate_swapchain(const VkExtent2D& dimension, bool vsync);

    VkInstance                   mInstance;
    VkPhysicalDevice             mPhysicalDevice;
    VkDevice                     mDevice;
    VkSurfaceKHR                 mSurface;

    std::uint32_t                mQueueFamilyIndex;
    VkFormat                     mColorFormat;
    VkColorSpaceKHR              mColorSpace;
    VkCommandPool                mCommandPool;

    VkSwapchainKHR               mSwapChain = VK_NULL_HANDLE;

    struct Buffer {
        VkImage     image;
        VkImageView view;
    };

    std::uint32_t                mImageCount;
    std::vector<VkImage>         mImages;
    std::vector<Buffer>          mBuffers;
    std::vector<VkCommandBuffer> mCommandBuffers;
    std::vector<VkFence>         mWaitFences;
};
