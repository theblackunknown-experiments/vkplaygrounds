#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <span>
#include <vector>

#include "./vkutilities.hpp"

namespace blk
{

struct Queue;
struct Engine;
struct Surface;

struct Presentation
{
    struct Image
    {
        const std::uint32_t  index                  = ~0;
        VkCommandBuffer      commandbuffer          = VK_NULL_HANDLE;
        VkSemaphore          semaphore              = VK_NULL_HANDLE;
        VkPipelineStageFlags destination_stage_mask = 0;

        operator std::uint32_t() const
        {
            return index;
        }
    };

    explicit Presentation(
        const blk::Engine& vkengine,
        const blk::Surface& vksurface,
        const VkExtent2D& resolution);
    ~Presentation();

    VkExtent2D recreate_swapchain();

    [[nodiscard]] Image acquire_next(std::uint64_t timeout);
    [[nodiscard]] VkResult present(const Image&, VkSemaphore wait_semaphore);

    void onResize(const VkExtent2D& resolution);

    const blk::Engine&           mEngine;
    const blk::Surface&          mSurface;
    VkExtent2D                   mResolution;

    VkDevice                     mDevice;
    VkPhysicalDevice             mPhysicalDevice;

    std::vector<blk::Queue*>     mPresentationQueues;

    VkFormat                     mColorFormat   = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR              mColorSpace    = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR             mPresentMode   = VK_PRESENT_MODE_MAX_ENUM_KHR;

    // FIXME To scale, we would need to have an array of semaphore present/complete if we want to process frame as fast as possible
    VkSemaphore                  mAcquiredSemaphore = VK_NULL_HANDLE;
    VkCommandPool                mCommandPool       = VK_NULL_HANDLE;

    std::uint32_t                mImageCount    = 0;
    VkSwapchainKHR               mSwapchain     = VK_NULL_HANDLE;
    std::vector<VkImage>         mImages        ;
    std::vector<VkCommandBuffer> mCommandBuffers;
};

}

