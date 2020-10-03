#pragma once

#include <vulkan/vulkan_core.h>

#include "./vkdebug.hpp"

namespace blk
{
    struct Device;

    struct RenderPass
    {
        const blk::Device& mDevice;
        VkRenderPass       mRenderPass = VK_NULL_HANDLE;

        constexpr RenderPass(const blk::Device& vkdevice)
            : mDevice(vkdevice)
        {
        }

        ~RenderPass()
        {
            destroy();
        }

        VkResult create(const VkRenderPassCreateInfo& info);

        void destroy();

        constexpr bool created() const
        {
            return mRenderPass != VK_NULL_HANDLE;
        }

        constexpr operator VkRenderPass() const
        {
            return mRenderPass;
        }
    };
}
