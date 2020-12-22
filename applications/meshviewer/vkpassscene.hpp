#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

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

namespace blk::meshviewer
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
            VkExtent2D   resolution;
        };

        // TODO Re-use pipeline cache object, instead of one per pass
        explicit PassScene(const blk::RenderPass& renderpass, std::uint32_t subpass, Arguments arguments);
        ~PassScene();

        void initialize_graphic_pipelines();

        void record_pass(VkCommandBuffer commandbuffer) override;

        void onResize(const VkExtent2D& resolution);

        blk::Engine&                         mEngine;
        const blk::Device&                   mDevice;
        VkExtent2D                           mResolution;
        ImGuiContext*                        mContext = nullptr;

        VkPipelineLayout                     mPipelineLayout               = VK_NULL_HANDLE;

        VkPipelineCache                      mPipelineCache                = VK_NULL_HANDLE;

        VkPipeline                           mPipeline                     = VK_NULL_HANDLE;
    };

}
