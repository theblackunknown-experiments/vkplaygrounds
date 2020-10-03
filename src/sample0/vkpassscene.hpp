#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <array>
#include <chrono>
#include <vector>

#include "./vkdevice.hpp"
#include "./vkrenderpass.hpp"
#include "./vkbuffer.hpp"
#include "./vkimage.hpp"

#include "./vkpass.hpp"

struct ImGuiContext;

namespace blk
{
    struct Engine;
}

namespace blk::sample0
{
    struct PassScene : Pass
    {
        using frame_clock_t         = std::chrono::high_resolution_clock;
        using frame_tick_t          = frame_clock_t::time_point;
        using frame_time_delta_s_t  = std::chrono::duration<float/*, std::seconds*/>;
        using frame_time_delta_ms_t = std::chrono::duration<float, std::milli>;

        struct Arguments
        {
            blk::Engine& engine;
        };

        // TODO Re-use pipeline cache object, instead of one per pass
        // TODO Give a RenderPass object
        explicit PassScene(const blk::RenderPass& renderpass, std::uint32_t subpass, Arguments arguments);
        // explicit PassScene(
        //     const blk::Device&     device,
        //     const blk::RenderPass& renderpass,
        //     std::uint32_t          subpass,
        //     const VkExtent2D&      resolution);
        ~PassScene();

        void initialize_resolution(const VkExtent2D& resolution);
        void initialize_before_memory_bindings();
        void initialize_after_memory_bindings();
        void initialize_graphic_pipelines();

        void render_frame();

        void upload_font_image(blk::Buffer& staging_buffer);
        void upload_frame_buffers();

        void record_pass(VkCommandBuffer commandbuffer) override;
        void record_frame(VkCommandBuffer commandbuffer);
        void record_font_image_upload(VkCommandBuffer commandbuffer, const blk::Buffer& staging_buffer);

        const blk::Device&                   mDevice;
        VkExtent2D                           mResolution;
        ImGuiContext*                        mContext = nullptr;

        VkExtent3D                           mFontExtent;
        blk::Image                           mFontImage;
        blk::ImageView                       mFontImageView;
        blk::Buffer                          mVertexBuffer, mIndexBuffer;
        VkSampler                            mSampler                      = VK_NULL_HANDLE;

        VkDescriptorSetLayout                mDescriptorSetLayout          = VK_NULL_HANDLE;
        VkPipelineLayout                     mPipelineLayout               = VK_NULL_HANDLE;

        VkPipelineCache                      mPipelineCache                = VK_NULL_HANDLE;

        std::array<VkPipeline, 1>            mPipelines;

        VkDescriptorPool                     mDescriptorPool               = VK_NULL_HANDLE;
        VkDescriptorSet                      mDescriptorSet                = VK_NULL_HANDLE;

        struct UI {
            frame_time_delta_ms_t frame_delta = frame_time_delta_ms_t(0.f);
            frame_tick_t          count_tick;

            std::uint32_t         frame_count    = 0;
            std::array<float, 50> frame_times    = {};
            float                 frame_time_min;
            float                 frame_time_max;

            bool                  show_gpu_information = false;
            bool                  show_fps = false;
            bool                  show_demo = false;
        } mUI;

        struct Mouse
        {
            struct Buttons
            {
                bool left   = false;
                bool middle = false;
                bool right  = false;
            } buttons;
            struct Positions
            {
                float x = 0.0f;
                float y = 0.0f;
            } offset;
        } mMouse;


        static_assert(std::chrono::treat_as_floating_point_v<frame_time_delta_s_t::rep>, "required to be floating point");
        static_assert(std::chrono::treat_as_floating_point_v<frame_time_delta_ms_t::rep>, "required to be floating point");
    };

}
