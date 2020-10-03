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
    explicit Presentation(
        const blk::Engine& vkengine,
        const blk::Surface& vksurface,
        const VkExtent2D& resolution);
    ~Presentation();

    void create_swapchain();
    void create_framebuffers();
    void create_commandbuffers();

    AcquiredPresentationImage acquire();

    void record(AcquiredPresentationImage& );
    void present(const AcquiredPresentationImage& );

    void render_frame();

    void wait_staging_operations();

    const blk::Engine&                   mEngine;
    const blk::Surface&                  mSurface;
    VkExtent2D                           mResolution;

    VkDevice                             mDevice;
    VkPhysicalDevice                     mPhysicalDevice;

    std::vector<blk::Queue*>             mPresentationQueues;

    VkFormat                             mColorFormat   = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR                      mColorSpace    = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR                     mPresentMode   = VK_PRESENT_MODE_MAX_ENUM_KHR;

    // FIXME To scale, we would need to have an array of semaphore present/complete if we want to process frame as fast as possible
    VkSemaphore                          mAcquiredSemaphore = VK_NULL_HANDLE;
    VkSemaphore                          mRenderSemaphore   = VK_NULL_HANDLE;
    VkCommandPool                        mCommandPool       = VK_NULL_HANDLE;

    VkSwapchainKHR                       mSwapchain     = VK_NULL_HANDLE;
    std::vector<VkImage>                 mImages        ;
    std::vector<VkImageView>             mViews         ;
    std::vector<VkCommandBuffer>         mCommandBuffers;

    // FIXME API entry point for framebuffer
    std::vector<VkFramebuffer>           mFrameBuffers;
};

}

