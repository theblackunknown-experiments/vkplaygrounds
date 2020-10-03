#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <array>
#include <chrono>
#include <vector>

#include "./vkutilities.hpp"
#include "./vkphysicaldevice.hpp"
#include "./vkdevice.hpp"
#include "./vkbuffer.hpp"
#include "./vkimage.hpp"

struct ImGuiContext;

struct DearImGuiShowcase
{
    using frame_clock_t         = std::chrono::high_resolution_clock;
    using frame_tick_t          = frame_clock_t::time_point;
    using frame_time_delta_s_t  = std::chrono::duration<float/*, std::seconds*/>;
    using frame_time_delta_ms_t = std::chrono::duration<float, std::milli>;

    explicit DearImGuiShowcase(
        VkInstance vkinstance,
        VkSurfaceKHR vksurface,
        VkPhysicalDevice vkphysicaldevice,
        const VkExtent2D& resolution);
    ~DearImGuiShowcase();

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
    blk::PhysicalDevice                  mPhysicalDevice;

    VkExtent2D                           mResolution;


    std::uint32_t                        mQueueFamily;
    blk::Device                          mDevice;
    VkQueue                              mQueue                        = VK_NULL_HANDLE;

    ImGuiContext*                        mContext                      = nullptr;

    VkColorSpaceKHR                      mColorSpace                   = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR                     mPresentMode                  = VK_PRESENT_MODE_MAX_ENUM_KHR;

    VkFormat                             mDepthStencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    VkFormat                             mColorAttachmentFormat        = VK_FORMAT_UNDEFINED;

    VkSampler                            mSampler                      = VK_NULL_HANDLE;

    VkRenderPass                         mRenderPass                   = VK_NULL_HANDLE;

    VkDescriptorSetLayout                mDescriptorSetLayout          = VK_NULL_HANDLE;
    VkPipelineLayout                     mUIPipelineLayout             = VK_NULL_HANDLE;
    VkPipelineLayout                     mScenePipelineLayout          = VK_NULL_HANDLE;

    VkPipelineCache                      mPipelineCache                = VK_NULL_HANDLE;

    std::array<VkPipeline, 2>            mPipelines;

    VkCommandPool                        mCommandPool                  = VK_NULL_HANDLE;
    VkCommandPool                        mCommandPoolOneOff            = VK_NULL_HANDLE;
    VkDescriptorPool                     mDescriptorPool               = VK_NULL_HANDLE;

    // NOTE To scale, we would need to have an array of semaphore present/complete if we want to process frame as fast as possible
    VkSemaphore                          mAcquiredSemaphore            = VK_NULL_HANDLE;
    VkSemaphore                          mRenderSemaphore              = VK_NULL_HANDLE;

    VkDescriptorSet                      mDescriptorSet                = VK_NULL_HANDLE;

    blk::Image                           mFontImage, mDepthImage;
    blk::ImageView                       mFontImageView, mDepthImageView;
    blk::Buffer                          mVertexBuffer, mIndexBuffer, mStagingBuffer;

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
