#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <vector>
#include <array>

#include "./vkdevice.hpp"
#include "./vkrenderpass.hpp"
#include "./vkbuffer.hpp"
#include "./vkqueue.hpp"

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

        blk::Buffer                          mVertexBuffer;
        blk::Buffer                          mIndexBuffer;
        blk::Buffer                          mStagingBuffer;

        std::unique_ptr<blk::Memory>         mGeometryMemory;
        std::unique_ptr<blk::Memory>         mStagingMemory;

        blk::Queue*                          mGraphicsQueue = nullptr;

        VkCommandPool                        mGraphicsCommandPoolTransient = VK_NULL_HANDLE;

        VkCommandBuffer                      mStagingCommandBuffer = VK_NULL_HANDLE;
        VkFence                              mStagingFence         = VK_NULL_HANDLE;
        VkSemaphore                          mStagingSemaphore     = VK_NULL_HANDLE;

    };

}
