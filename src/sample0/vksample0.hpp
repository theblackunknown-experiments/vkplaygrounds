#pragma once

#include <vulkan/vulkan_core.h>

#include "../vkpass.hpp"
#include "../vkrenderpass.hpp"

#include "../vkimage.hpp"

#include "./vkpassscene.hpp"
#include "./vkpassuioverlay.hpp"

#include <span>
#include <memory>

namespace blk
{
    struct Engine;
    struct Device;
    struct Memory;
}

namespace blk::sample0
{
    struct Sample
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

        Sample(
            blk::Engine& vkengine,
            VkFormat formatColor,
            const std::span<VkImage>& backbufferimages,
            const VkExtent2D& resolution
        );
        ~Sample();

        void render_imgui_frame();
        void record(std::uint32_t backbufferindex, VkCommandBuffer commandbuffer);
    };
}
