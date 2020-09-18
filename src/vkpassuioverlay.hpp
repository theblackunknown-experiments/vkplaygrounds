#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <array>
#include <chrono>
#include <vector>

#include "./vkutilities.hpp"
#include "./vkdevice.hpp"
#include "./vkrenderpass.hpp"
#include "./vkbuffer.hpp"
#include "./vkimage.hpp"

struct ImGuiContext;

struct VulkanPassUIOverlay
{
    using frame_clock_t         = std::chrono::high_resolution_clock;
    using frame_tick_t          = frame_clock_t::time_point;
    using frame_time_delta_s_t  = std::chrono::duration<float/*, std::seconds*/>;
    using frame_time_delta_ms_t = std::chrono::duration<float, std::milli>;

    // TODO Re-use pipeline cache object, instead of one per pass
    // TODO Give a RenderPass object
    explicit VulkanPassUIOverlay(
        const blk::Device&     device,
        const blk::RenderPass& renderpass,
        std::uint32_t          subpass,
        const VkExtent2D&      resolution);
    ~VulkanPassUIOverlay();

    void initialize_before_memory_bindings();
    void initialize_after_memory_bindings();
    void initialize_graphic_pipelines();

    void record_font_image_upload(VkCommandBuffer commandbuffer);

    AcquiredPresentationImage acquire();

    void record(AcquiredPresentationImage& );
    void present(const AcquiredPresentationImage& );

    void render_frame();

    void wait_pending_operations();

    const blk::Device&                   mDevice;
    const blk::RenderPass&               mRenderPass;
    std::uint32_t                        mSubpass;
    VkExtent2D                           mResolution;
    ImGuiContext*                        mContext = nullptr;

    // VkFormat                             mDepthStencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    // VkFormat                             mColorAttachmentFormat        = VK_FORMAT_UNDEFINED;
    VkExtent3D                           mFontExtent;
    blk::Image                           mFontImage, mDepthImage;
    blk::ImageView                       mFontImageView, mDepthImageView;
    blk::Buffer                          mVertexBuffer, mIndexBuffer, mStagingBuffer;
    VkSampler                            mSampler                      = VK_NULL_HANDLE;

    VkDescriptorSetLayout                mDescriptorSetLayout          = VK_NULL_HANDLE;
    VkPipelineLayout                     mPipelineLayout               = VK_NULL_HANDLE;

    VkPipelineCache                      mPipelineCache                = VK_NULL_HANDLE;

    std::array<VkPipeline, 1>            mPipelines;

    VkDescriptorPool                     mDescriptorPool               = VK_NULL_HANDLE;
    VkDescriptorSet                      mDescriptorSet                = VK_NULL_HANDLE;

    VkFence                              mStagingFence = VK_NULL_HANDLE;
    VkSemaphore                          mStagingSemaphore = VK_NULL_HANDLE;
    VkCommandBuffer                      mStagingCommandBuffer = VK_NULL_HANDLE;

    std::vector<blk::Memory>             mMemoryChunks;

    PresentationData                     mPresentation;

    std::vector<VkFramebuffer>           mFrameBuffers;
    std::vector<VkCommandBuffer>         mCommandBuffers;

    struct UI {
        frame_time_delta_ms_t frame_delta = frame_time_delta_ms_t(0.f);
        frame_tick_t          count_tick;

        std::uint32_t         frame_count    = 0;
        std::array<float, 50> frame_times    = {};
        float                 frame_time_min;
        float                 frame_time_max;

        bool                                  show_gpu_information = false;
        bool                                  show_fps = false;
        bool                                  show_demo = false;
    } mUI;

    Mouse mMouse;


    static_assert(std::chrono::treat_as_floating_point_v<frame_time_delta_s_t::rep>, "required to be floating point");
    static_assert(std::chrono::treat_as_floating_point_v<frame_time_delta_ms_t::rep>, "required to be floating point");
};
