#include <vulkan/vulkan.h>

#include <cassert>

#include <vector>
#include <limits>
#include <optional>
#include <algorithm>

#include "vulkandebug.hpp"

#include "vulkansurface.hpp"

VulkanSurface::VulkanSurface(
        const VkInstance& instance,
        const VkPhysicalDevice& physical_device,
        const VkDevice& device,
        const VkSurfaceKHR& surface,
        const VkExtent2D& resolution)
    : VulkanPhysicalDeviceBase(physical_device)
    , VulkanSurfaceBase(surface)
    , VulkanSurfaceMixin()
    , mInstance(instance)
    , mPhysicalDevice(physical_device)
    , mDevice(device)
    , mSurface(surface)
    , mResolution(resolution)
{
    {// Queue Family
        auto index = select_surface_queue_family_index(mSurface);
        assert(index.has_value());
        mQueueFamilyIndex = *index;
    }
    {// Formats
        std::uint32_t count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &count, nullptr);
        assert(count > 0);

        std::vector<VkSurfaceFormatKHR> formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &count, formats.data());

        // lowest denominator
        if((count == 1) && (formats.front().format == VK_FORMAT_UNDEFINED))
        {
            mColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
            mColorSpace  = formats.front().colorSpace;
        }
        // otherwise looks for our preferred one, or switch back to the 1st available
        else
        {
            auto finder = std::find_if(
                std::begin(formats), std::end(formats),
                [](const VkSurfaceFormatKHR& f) {
                    return f.format == VK_FORMAT_R8G8B8A8_UNORM;
                }
            );

            if(finder != std::end(formats))
            {
                const VkSurfaceFormatKHR& preferred_format = *finder;
                mColorFormat = preferred_format.format;
                mColorSpace  = preferred_format.colorSpace;
            }
            else
            {
                mColorFormat = formats.front().format;
                mColorSpace  = formats.front().colorSpace;
            }
        }
    }
    {// Queue
        vkGetDeviceQueue(mDevice, mQueueFamilyIndex, 0, &mQueue);
    }
    {// Command Pool
        VkCommandPoolCreateInfo info_pool{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = mQueueFamilyIndex,
        };
        CHECK(vkCreateCommandPool(mDevice, &info_pool, nullptr, &mCommandPool));
    }
    {
        VkSemaphoreCreateInfo info_semaphore{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mSemaphorePresentComplete));
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mSemaphoreRenderComplete));
    }
}

VulkanSurface::~VulkanSurface()
{
    for (VkFence& fence : mWaitFences)
    {
        vkDestroyFence(mDevice, fence, nullptr);
    }

    vkFreeCommandBuffers(mDevice, mCommandPool, static_cast<uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());

    for (Buffer& buffer : mBuffers)
    {
        vkDestroyImageView(mDevice, buffer.view, nullptr);
    }

    vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

    vkDestroySemaphore(mDevice, mSemaphorePresentComplete, nullptr);
    vkDestroySemaphore(mDevice, mSemaphoreRenderComplete, nullptr);
}

void VulkanSurface::generate_swapchain(bool vsync)
{
    VkSwapchainKHR previous_swap_chain = mSwapChain;

    // Dimension
    VkSurfaceCapabilitiesKHR capabilities;
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &capabilities));

    mResolution = (capabilities.currentExtent.width == static_cast<std::uint32_t>(0xFFFFFFFF))
        ? mResolution
        : capabilities.currentExtent;

    { // Swap Chain
        // Present modes
        std::uint32_t count;
        CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &count, nullptr));
        assert(count > 0);

        std::vector<VkPresentModeKHR> present_modes(count);
        CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &count, present_modes.data()));

        auto finder_present_mode = std::find(std::begin(present_modes), std::end(present_modes), VK_PRESENT_MODE_FIFO_KHR);
        assert(finder_present_mode != std::end(present_modes));

        if(!vsync)
        {
            auto finder_present_mode_better = std::find(
                std::begin(present_modes), std::end(present_modes),
                VK_PRESENT_MODE_MAILBOX_KHR
            );
            if (finder_present_mode_better == std::end(present_modes))
            {
                finder_present_mode_better = std::find(
                    std::begin(present_modes), std::end(present_modes),
                    VK_PRESENT_MODE_IMMEDIATE_KHR
                );
            }

            if (finder_present_mode_better != std::end(present_modes))
            {
                finder_present_mode = finder_present_mode_better;
            }
        }
        assert(finder_present_mode != std::end(present_modes));

        VkPresentModeKHR present_mode = *finder_present_mode;

        std::uint32_t image_count = (capabilities.maxImageCount > 0)
            ? std::min(capabilities.minImageCount + 1, capabilities.maxImageCount)
            : capabilities.minImageCount + 1;

        VkSurfaceTransformFlagBitsKHR transform = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
            : capabilities.currentTransform;

        constexpr const VkCompositeAlphaFlagBitsKHR kCompositeAlphaFlags[] = {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        auto finder_composite_alpha = std::find_if(
            std::begin(kCompositeAlphaFlags), std::end(kCompositeAlphaFlags),
            [&capabilities](const VkCompositeAlphaFlagBitsKHR& f) {
                return capabilities.supportedCompositeAlpha & f;
            }
        );
        assert(finder_composite_alpha != std::end(kCompositeAlphaFlags));

        VkCompositeAlphaFlagBitsKHR composite_alpha = *finder_composite_alpha;

        VkSwapchainCreateInfoKHR info_swapchain{
            .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext                 = nullptr,
            .flags                 = 0,
            .surface               = mSurface,
            .minImageCount         = image_count,
            .imageFormat           = mColorFormat,
            .imageColorSpace       = mColorSpace,
            .imageExtent           = mResolution,
            .imageArrayLayers      = 1,
            .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .preTransform          = transform,
            .compositeAlpha        = composite_alpha,
            .presentMode           = present_mode,
            .clipped               = VK_TRUE,
            .oldSwapchain          = previous_swap_chain,
        };

        if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            info_swapchain.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            info_swapchain.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        CHECK(vkCreateSwapchainKHR(mDevice, &info_swapchain, nullptr, &mSwapChain));
    }

    if (previous_swap_chain != VK_NULL_HANDLE)
    {
        for(auto idx = 0u, size = mCount; idx < size; ++idx)
            vkDestroyImageView(mDevice, mBuffers.at(idx).view, nullptr);
        vkDestroySwapchainKHR(mDevice, previous_swap_chain, nullptr);
    }

    {// Views
        CHECK(vkGetSwapchainImagesKHR(mDevice, mSwapChain, &mCount, nullptr));
        mImages.resize(mCount);
        CHECK(vkGetSwapchainImagesKHR(mDevice, mSwapChain, &mCount, mImages.data()));

        mBuffers.resize(mCount);

        for (auto idx = 0u, size = mCount; idx < size; ++idx)
        {
            mBuffers.at(idx).image = mImages.at(idx);
            const VkImageViewCreateInfo info_view{
                .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .image            = mBuffers.at(idx).image,
                .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                .format           = mColorFormat,
                .components       = VkComponentMapping{
                    .r = VK_COMPONENT_SWIZZLE_R,
                    .g = VK_COMPONENT_SWIZZLE_G,
                    .b = VK_COMPONENT_SWIZZLE_B,
                    .a = VK_COMPONENT_SWIZZLE_A,
                },
                .subresourceRange = VkImageSubresourceRange{
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
            };
            CHECK(vkCreateImageView(mDevice, &info_view, nullptr, &mBuffers.at(idx).view));
        }
    }
    {// Command Buffers
        mCommandBuffers.resize(mCount);

        const VkCommandBufferAllocateInfo info_allocation{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = mCommandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = mCount,
        };
        CHECK(vkAllocateCommandBuffers(mDevice, &info_allocation, mCommandBuffers.data()));
    }
    {// Fences
        mWaitFences.resize(mCount);

        const VkFenceCreateInfo info_fence{
            .sType              = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext              = nullptr,
            .flags              = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        for(VkFence& fence : mWaitFences)
            CHECK(vkCreateFence(mDevice, &info_fence, nullptr, &fence));
    }
}

std::uint32_t VulkanSurface::next_index()
{
    // TODO Handle VK_ERROR_OUT_OF_DATE_KHR
    // TODO Handle VK_SUBOPTIMAL_KHR
    std::uint32_t idx = 0;
    CHECK(vkAcquireNextImageKHR(
        mDevice,
        mSwapChain,
        std::numeric_limits<std::uint64_t>::max(),
        mSemaphorePresentComplete,
        VK_NULL_HANDLE,
        &idx
    ));

    return idx;
}

void VulkanSurface::submit(std::uint32_t idx)
{
    VkCommandBuffer& cmdbuffer = mCommandBuffers.at(idx);
    const VkSubmitInfo info{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = nullptr,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &mSemaphorePresentComplete,
        .pWaitDstStageMask    = &mPipelineStageSubmission,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmdbuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &mSemaphoreRenderComplete,
    };
    CHECK(vkQueueSubmit(mQueue, 1, &info, VK_NULL_HANDLE));
}

void VulkanSurface::present(std::uint32_t idx)
{
    const VkPresentInfoKHR info{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext              = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &mSemaphoreRenderComplete,
        .swapchainCount     = 1,
        .pSwapchains        = &mSwapChain,
        .pImageIndices      = &idx,
        .pResults           = nullptr,
    };
    // TODO Handle VK_ERROR_OUT_OF_DATE_KHR
    // TODO Handle VK_SUBOPTIMAL_KHR
    CHECK(vkQueuePresentKHR(mQueue, &info));

    // NOTE Can we avoid blocking ?
    CHECK(vkQueueWaitIdle(mQueue));
}
