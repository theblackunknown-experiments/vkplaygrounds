#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <array>
#include <chrono>
#include <vector>
#include <memory>

#include "./vkdevice.hpp"
#include "./vkrenderpass.hpp"
#include "./vkbuffer.hpp"
#include "./vkimage.hpp"

#include "./vkpass.hpp"

struct ImGuiContext;

namespace blk
{
    struct Engine;
    struct Memory;
}

namespace blk::sample0
{
    struct PassUIOverlay : Pass
    {
        using frame_clock_t         = std::chrono::high_resolution_clock;
        using frame_tick_t          = frame_clock_t::time_point;
        using frame_time_delta_s_t  = std::chrono::duration<float/*, std::seconds*/>;
        using frame_time_delta_ms_t = std::chrono::duration<float, std::milli>;

        struct Arguments
        {
            blk::Engine& engine;
            VkExtent2D   resolution;
        };

        // TODO Re-use pipeline cache object, instead of one per pass
        explicit PassUIOverlay(const blk::RenderPass& renderpass, std::uint32_t subpass, Arguments arguments);
        ~PassUIOverlay();

        void initialize_graphic_pipelines();

        void render_frame();

        void upload_frame_buffers();
        void upload_font_image(blk::Buffer& staging_buffer);

        void record_pass(VkCommandBuffer commandbuffer) override;
        void record_font_image_upload(VkCommandBuffer commandbuffer, const blk::Buffer& staging_buffer);

        blk::Engine&                         mEngine;
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

        VkPipeline                           mPipeline                     = VK_NULL_HANDLE;

        VkDescriptorPool                     mDescriptorPool               = VK_NULL_HANDLE;
        VkDescriptorSet                      mDescriptorSet                = VK_NULL_HANDLE;

        std::unique_ptr<blk::Memory>         mGeometryMemory;
        std::unique_ptr<blk::Memory>         mIndexMemory;
        std::unique_ptr<blk::Memory>         mVertexMemory;
        std::unique_ptr<blk::Memory>         mFontMemory;

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
