#pragma once

#include <vulkan/vulkan_core.h>

#include "./vkdebug.hpp"

namespace blk
{
    struct RenderPass
    {
        VkRenderPass mRenderPass = VK_NULL_HANDLE;
        VkDevice     mDevice = VK_NULL_HANDLE;

        constexpr RenderPass() = default;

        constexpr RenderPass(const RenderPass& rhs) = delete;

        constexpr RenderPass(RenderPass&& rhs)
            : mRenderPass(std::exchange(rhs.mRenderPass, VkRenderPass{VK_NULL_HANDLE}))
            , mDevice(std::exchange(rhs.mDevice, VkDevice{VK_NULL_HANDLE}))
        {
        }

        ~RenderPass()
        {
            destroy();
        }

        RenderPass& operator=(const RenderPass& rhs) = delete;

        RenderPass& operator=(RenderPass&& rhs)
        {
            mRenderPass = std::exchange(rhs.mRenderPass, mRenderPass);
            return *this;
        }

        VkResult create(VkDevice vkdevice, const VkRenderPassCreateInfo& info)
        {
            mDevice = vkdevice;
            auto result = vkCreateRenderPass(mDevice, &info, nullptr, &mRenderPass);
            CHECK(result);
            return result;
        }

        void destroy()
        {
            if (mRenderPass != VK_NULL_HANDLE)
                vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        }

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
