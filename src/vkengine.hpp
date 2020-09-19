#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <span>
#include <tuple>

#include <array>
#include <chrono>
#include <vector>

#include "./vkutilities.hpp"
#include "./vkdevice.hpp"

#include "./vkbuffer.hpp"
#include "./vkimage.hpp"

namespace blk
{

struct Queue;
struct Image;
struct Memory;
struct Buffer;
struct ImageView;
struct PhysicalDevice;

struct Engine
{
    explicit Engine(
        VkInstance vkinstance,
        VkSurfaceKHR vksurface,
        const blk::PhysicalDevice& vkphysicaldevice,
        const std::span<VkDeviceQueueCreateInfo>& info_queues,
        const VkExtent2D& resolution);
    ~Engine();

    void initialize();
    void initialize_views();
    void initialize_resources();

    void allocate_memory_and_bind_resources(
        const std::span<std::tuple<blk::Buffer*, VkMemoryPropertyFlags>>& buffers,
        const std::span<std::tuple<blk::Image*, VkMemoryPropertyFlags>>& images);

    void create_swapchain();
    void create_framebuffers();
    void create_commandbuffers();

    AcquiredPresentationImage acquire();

    void record(AcquiredPresentationImage& );
    void present(const AcquiredPresentationImage& );

    void render_frame();

    void wait_staging_operations();

    VkInstance                           mInstance                     = VK_NULL_HANDLE;
    VkSurfaceKHR                         mSurface                      = VK_NULL_HANDLE;
    const blk::PhysicalDevice&           mPhysicalDevice;
    VkExtent2D                           mResolution;

    blk::Device                          mDevice;

    std::vector<blk::Queue>              mQueues;
    std::vector<blk::Queue*>             mSparseQueues;
    std::vector<blk::Queue*>             mComputeQueues;
    std::vector<blk::Queue*>             mTransferQueues;
    std::vector<blk::Queue*>             mGraphicsQueues;
    std::vector<blk::Queue*>             mPresentationQueues;

    VkColorSpaceKHR                      mColorSpace                   = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR                     mPresentMode                  = VK_PRESENT_MODE_MAX_ENUM_KHR;

    VkFormat                             mDepthStencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    VkFormat                             mColorAttachmentFormat        = VK_FORMAT_UNDEFINED;

    // FIXME API entry point for renderpass
    VkRenderPass                         mRenderPass                   = VK_NULL_HANDLE;

    VkPipelineCache                      mPipelineCache                = VK_NULL_HANDLE;

    VkCommandPool                        mComputeCommandPool           = VK_NULL_HANDLE;
    VkCommandPool                        mTransferCommandPool          = VK_NULL_HANDLE;
    VkCommandPool                        mGraphicsCommandPool          = VK_NULL_HANDLE;
    VkCommandPool                        mPresentationCommandPool      = VK_NULL_HANDLE;

    blk::Buffer                          mStagingBuffer;
    VkFence                              mStagingFence                 = VK_NULL_HANDLE;
    VkSemaphore                          mStagingSemaphore             = VK_NULL_HANDLE;
    VkCommandBuffer                      mStagingCommandBuffer         = VK_NULL_HANDLE;

    // FIXME Delegate to presentation
    // FIXME To scale, we would need to have an array of semaphore present/complete if we want to process frame as fast as possible
    VkSemaphore                          mAcquiredSemaphore            = VK_NULL_HANDLE;
    VkSemaphore                          mRenderSemaphore              = VK_NULL_HANDLE;

    // FIXME API entry point for resources
    blk::Image                           mDepthImage;
    blk::ImageView                       mDepthImageView;

    // FIXME API entry point for memory
    std::vector<blk::Memory>             mMemoryChunks;

    // FIXME API entry point for presentation
    PresentationData                     mPresentation;

    // FIXME API entry point for framebuffer
    std::vector<VkFramebuffer>           mFrameBuffers;
    std::vector<VkCommandBuffer>         mCommandBuffers;
};

}

