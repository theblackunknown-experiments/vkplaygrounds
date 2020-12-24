#pragma once

#include <vulkan/vulkan_core.h>

#include "./vkpass.hpp"
#include "./vkrenderpass.hpp"

#include "./vkimage.hpp"

#include "./vkpassscene.hpp"
#include "./vkpassuioverlay.hpp"

#include "./vkmeshviewerdata.hpp"

#include <span>
#include <memory>

#include <filesystem>

namespace fs = std::filesystem;

namespace blk
{
    struct Engine;
    struct Device;
    struct Memory;
}

namespace blk::meshviewer
{
    struct MeshViewer
    {
        using multipass_type = MultiPass<
            PassTrait<PassUIOverlay, PassUIOverlay::Arguments>,
            PassTrait<PassScene    , PassScene    ::Arguments>
        >;

        struct RenderPass : ::blk::RenderPass
        {
            RenderPass(Engine& vkengine, VkFormat formatColor, VkFormat formatDepth);
        };

        Engine&                      mEngine;
        const Device&                mDevice;

        VkFormat                     mColorFormat;
        VkFormat                     mDepthFormat;

        VkExtent2D                   mResolution;
        SharedData                   mSharedData;

        RenderPass                   mRenderPass;
        multipass_type               mMultipass;

        PassUIOverlay&               mPassUIOverlay;
        PassScene&                   mPassScene;

        blk::Image                   mDepthImage;
        blk::ImageView               mDepthImageView;

        std::unique_ptr<blk::Memory> mDepthMemory;

        std::vector<VkSemaphore>     mRenderSemaphores;
        std::vector<VkImageView>     mBackBufferViews;
        std::vector<VkFramebuffer>   mFrameBuffers;

        MeshViewer(
            blk::Engine& vkengine,
            VkFormat formatColor,
            const std::span<VkImage>& backbufferimages,
            const VkExtent2D& resolution
        );
        ~MeshViewer();

        void recreate_depth();
        void recreate_backbuffers(VkFormat formatColor, const std::span<VkImage>& backbufferimages);

        void onIdle();
        void onResize(const VkExtent2D& resolution);
        void onKeyPressed(std::uint32_t backbufferindex, VkCommandBuffer commandbuffer);

        void record(std::uint32_t backbufferindex, VkCommandBuffer commandbuffer);
    };
}
