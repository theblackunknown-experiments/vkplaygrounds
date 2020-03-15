#pragma once

#include <vulkan/vulkan.h>

template<typename Base>
struct VulkanQueueMixin
{
    VkResult submit_command_buffer(VkCommandBuffer command_buffer)
    {
        const Base& that = static_cast<const Base&>(*this);
        const VkSubmitInfo info_submit{
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 0,
            .pWaitSemaphores      = nullptr,
            .pWaitDstStageMask    = nullptr,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &command_buffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores    = nullptr,
        };
        const VkFenceCreateInfo info_fence{
            .sType              = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext              = nullptr,
            .flags              = 0,
        };

        VkFence fence;
        CHECK(vkCreateFence(that.mDevice, &info_fence, nullptr, &fence));

        VkResult result = vkQueueSubmit(that.mQueue, 1, &info_submit, fence);

        CHECK(vkWaitForFences(that.mDevice, 1, &fence, VK_TRUE, 10e9));
        vkDestroyFence(that.mDevice, fence, nullptr);

        return result;
    }
};
