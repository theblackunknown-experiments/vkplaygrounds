#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

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

    static
    bool isSuitable(VkPhysicalDevice vkphysicaldevice, VkSurfaceKHR vksurface);

    void initialize();
    void initialize_resources();
    void initialize_views();

    void allocate_memory_and_bind_resources();
    void allocate_descriptorset();

    void upload_font_image();

    void create_swapchain();
    void create_framebuffers();
    void create_commandbuffers();
    void create_graphic_pipelines();

    // void reallocate_buffers();

    void update_descriptorset();

    AcquiredPresentationImage acquire();

    void record(AcquiredPresentationImage& );
    void present(const AcquiredPresentationImage& );

    void render_frame();

    void wait_pending_operations();

    VkInstance                           mInstance                     = VK_NULL_HANDLE;
    VkSurfaceKHR                         mSurface                      = VK_NULL_HANDLE;
    const blk::PhysicalDevice&           mPhysicalDevice;

    VkExtent2D                           mResolution;

    std::vector<blk::Queue>              mQueues;
    std::vector<blk::Queue*>             mComputeQueues;
    std::vector<blk::Queue*>             mTransferQueues;
    std::vector<blk::Queue*>             mGraphicsQueues;
    std::vector<blk::Queue*>             mPresentationQueues;

    std::uint32_t                        mQueueFamily;
    blk::Device                          mDevice;
    // VkQueue                              mQueue                        = VK_NULL_HANDLE;

    // VkColorSpaceKHR                      mColorSpace                   = VK_COLOR_SPACE_MAX_ENUM_KHR;
    // VkPresentModeKHR                     mPresentMode                  = VK_PRESENT_MODE_MAX_ENUM_KHR;

    // VkFormat                             mDepthStencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    // VkFormat                             mColorAttachmentFormat        = VK_FORMAT_UNDEFINED;

    // VkSampler                            mSampler                      = VK_NULL_HANDLE;

    // VkRenderPass                         mRenderPass                   = VK_NULL_HANDLE;

    // VkDescriptorSetLayout                mDescriptorSetLayout          = VK_NULL_HANDLE;
    // VkPipelineLayout                     mUIPipelineLayout             = VK_NULL_HANDLE;
    // VkPipelineLayout                     mScenePipelineLayout          = VK_NULL_HANDLE;

    // VkPipelineCache                      mPipelineCache                = VK_NULL_HANDLE;

    // std::array<VkPipeline, 2>            mPipelines;

    // VkCommandPool                        mCommandPool                  = VK_NULL_HANDLE;
    // VkCommandPool                        mCommandPoolOneOff            = VK_NULL_HANDLE;
    // VkDescriptorPool                     mDescriptorPool               = VK_NULL_HANDLE;

    // // NOTE To scale, we would need to have an array of semaphore present/complete if we want to process frame as fast as possible
    // VkSemaphore                          mAcquiredSemaphore     = VK_NULL_HANDLE;
    // VkSemaphore                          mRenderSemaphore      = VK_NULL_HANDLE;

    // VkDescriptorSet                      mDescriptorSet                = VK_NULL_HANDLE;

    // blk::Image                           mDepthImage;
    // blk::ImageView                       mDepthImageView;
    // blk::Buffer                          mStagingBuffer;

    // VkFence                              mStagingFence = VK_NULL_HANDLE;
    // VkSemaphore                          mStagingSemaphore = VK_NULL_HANDLE;
    // VkCommandBuffer                      mStagingCommandBuffer = VK_NULL_HANDLE;

    // std::vector<blk::Memory>             mMemoryChunks;

    // PresentationData                     mPresentation;

    // std::vector<VkFramebuffer>           mFrameBuffers;
    // std::vector<VkCommandBuffer>         mCommandBuffers;
};

}

