#include "./vkrenderpass.hpp"

#include "./vkdevice.hpp"

namespace blk
{
    VkResult RenderPass::create(const VkRenderPassCreateInfo& info)
    {
        auto result = vkCreateRenderPass(mDevice, &info, nullptr, &mRenderPass);
        CHECK(result);
        return result;
    }

    void RenderPass::destroy()
    {
        if (mRenderPass != VK_NULL_HANDLE)
            vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    }
}
