#pragma once

#include <vulkan/vulkan.h>

#include "vulkandebug.hpp"
#include "vulkansurface.hpp"

struct ScopedPresentableImage
{
    ScopedPresentableImage(
        VulkanSurface& surface,
        VkResult& result_acquire,
        VkResult& result_present
        )
        : mSurface(surface)
        , mIndex(0u)
        , mResultAcquire(result_acquire)
        , mResultPresent(result_present)
    {
        mResultAcquire = vkAcquireNextImageKHR(
            mSurface.mDevice,
            mSurface.mSwapChain,
            std::numeric_limits<std::uint64_t>::max(),
            mSurface.mSemaphorePresentComplete,
            VK_NULL_HANDLE,
            &mIndex
        );
    }

    ~ScopedPresentableImage()
    {
        const VkPresentInfoKHR info{
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &mSurface.mSemaphoreRenderComplete,
            .swapchainCount     = 1,
            .pSwapchains        = &mSurface.mSwapChain,
            .pImageIndices      = &mIndex,
            .pResults           = nullptr,
        };
        mResultPresent = vkQueuePresentKHR(mSurface.mQueue, &info);
    }

    VulkanSurface& mSurface;
    VkResult&      mResultAcquire;
    VkResult&      mResultPresent;
    std::uint32_t  mIndex;
};
