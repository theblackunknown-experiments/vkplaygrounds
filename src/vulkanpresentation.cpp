#include <vulkan/vulkan.h>

#include <span>
#include <vector>
#include <iterator>

#include <iostream>

#include "./vkdebug.hpp"
#include "./vkutilities.hpp"

#include "./vulkanpresentation.hpp"

bool VulkanPresentation::supports(VkPhysicalDevice vkphysicaldevice, VkSurfaceKHR vksurface)
{
    std::uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
    assert(count > 0);

    for (std::uint32_t idx = 0; idx < count; ++idx)
    {
        VkBool32 supported;
        CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vkphysicaldevice, idx, vksurface, &supported));

        if (supported == VK_TRUE)
            return true;
    }
    return false;
}

std::tuple<std::uint32_t, std::uint32_t> VulkanPresentation::requirements(VkPhysicalDevice vkphysicaldevice, VkSurfaceKHR vksurface)
{
    constexpr const std::uint32_t queue_count = 1;
    std::uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
    assert(count > 0);

    for (std::uint32_t idx = 0; idx < count; ++idx)
    {
        VkBool32 supported;
        CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vkphysicaldevice, idx, vksurface, &supported));

        if (supported == VK_TRUE)
            return std::make_tuple(idx, queue_count);
    }
    std::cerr << "No valid physical device supports VulkanPresentation" << std::endl;
    return std::make_tuple(0u, 0u);
}

VulkanPresentation::VulkanPresentation(
        VkPhysicalDevice vkphysicaldevice,
        VkDevice vkdevice,
        std::uint32_t queue_family_index,
        const std::span<VkQueue>& vkqueues,
        VkSurfaceKHR vksurface,
        VkExtent2D resolution,
        bool vsync
    )
    : mPhysicalDevice(vkphysicaldevice)
    , mDevice(vkdevice)
    , mQueueFamilyIndex(queue_family_index)
    , mQueue(vkqueues[0])
    , mSurface(vksurface)
    , mResolution(resolution)
{
    assert(vkqueues.size() == 1);
    vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mFeatures);
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &mProperties);
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);
    {// Formats
        std::uint32_t count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &count, nullptr);
        assert(count > 0);

        std::vector<VkSurfaceFormatKHR> formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &count, formats.data());

        // lowest denominator
        constexpr const VkFormat kPreferredFormat = VK_FORMAT_R8G8B8A8_UNORM;
        if((count == 1) && (formats.front().format == VK_FORMAT_UNDEFINED))
        {
            // NOTE No preferred format, pick what you want
            mColorFormat = kPreferredFormat;
            mColorSpace  = formats.front().colorSpace;
        }
        // otherwise looks for our preferred one, or switch back to the 1st available
        else
        {
            auto finder = std::find_if(
                std::begin(formats), std::end(formats),
                [kPreferredFormat](const VkSurfaceFormatKHR& f) {
                    return f.format == kPreferredFormat;
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

    {// Present modes
        std::uint32_t count;
        CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &count, nullptr));
        assert(count > 0);

        std::vector<VkPresentModeKHR> present_modes(count);
        CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &count, present_modes.data()));

        auto finder_present_mode = std::end(present_modes);

        if(vsync)
        {
            finder_present_mode = std::find(
                std::begin(present_modes), std::end(present_modes),
                VK_PRESENT_MODE_FIFO_KHR
            );
            assert(finder_present_mode != std::end(present_modes));
        }
        else
        {
            constexpr const auto kPreferredPresentModes = {
                VK_PRESENT_MODE_MAILBOX_KHR,
                VK_PRESENT_MODE_IMMEDIATE_KHR,
                VK_PRESENT_MODE_FIFO_KHR,
            };
            for (auto&& preferred : kPreferredPresentModes)
            {
                finder_present_mode = std::find(
                    std::begin(present_modes), std::end(present_modes),
                    preferred
                );

                if (finder_present_mode != std::end(present_modes))
                    break;
            }
        }
        assert(finder_present_mode != std::end(present_modes));

        mPresentMode = *finder_present_mode;
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
    {// Semaphores
        VkSemaphoreCreateInfo info_semaphore{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mSemaphorePresentComplete));
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mSemaphoreRenderComplete));
    }
}

VulkanPresentation::~VulkanPresentation()
{
    vkDestroySemaphore(mDevice, mSemaphoreRenderComplete, nullptr);
    vkDestroySemaphore(mDevice, mSemaphorePresentComplete, nullptr);

    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

    vkDestroyDevice(mDevice, nullptr);
}

void VulkanPresentation::recreate_swapchain()
{
    VkSwapchainKHR previous_swapchain = mSwapChain;

    // Dimension
    VkSurfaceCapabilitiesKHR capabilities;
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &capabilities));

    auto next_resolution = (capabilities.currentExtent.width == static_cast<std::uint32_t>(0xFFFFFFFF))
        ? mResolution
        : capabilities.currentExtent;

    {// Creation
        std::uint32_t image_count = (capabilities.maxImageCount > 0)
            ? std::min(capabilities.minImageCount + 1, capabilities.maxImageCount)
            : capabilities.minImageCount + 1;

        const VkSurfaceTransformFlagBitsKHR transform = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
            : capabilities.currentTransform;

        constexpr const VkCompositeAlphaFlagBitsKHR kPreferredCompositeAlphaFlags[] = {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        auto finder_composite_alpha = std::find_if(
            std::begin(kPreferredCompositeAlphaFlags), std::end(kPreferredCompositeAlphaFlags),
            [&capabilities](const VkCompositeAlphaFlagBitsKHR& f) {
                return capabilities.supportedCompositeAlpha & f;
            }
        );
        assert(finder_composite_alpha != std::end(kPreferredCompositeAlphaFlags));

        const VkCompositeAlphaFlagBitsKHR composite_alpha = *finder_composite_alpha;

        VkSwapchainCreateInfoKHR info{
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
            .presentMode           = mPresentMode,
            .clipped               = VK_TRUE,
            .oldSwapchain          = previous_swapchain,
        };

        if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        CHECK(vkCreateSwapchainKHR(mDevice, &info, nullptr, &mSwapChain));
    }

    if (previous_swapchain != VK_NULL_HANDLE)
    {
        for(auto idx = 0u, size = mCount; idx < size; ++idx)
            vkDestroyImageView(mDevice, mImageViews.at(idx), nullptr);
        vkDestroySwapchainKHR(mDevice, previous_swapchain, nullptr);
    }

    {// Views
        CHECK(vkGetSwapchainImagesKHR(mDevice, mSwapChain, &mCount, nullptr));
        mImages.resize(mCount);
        CHECK(vkGetSwapchainImagesKHR(mDevice, mSwapChain, &mCount, mImages.data()));

        mImageViews.resize(mCount);

        for (auto idx = 0u, size = mCount; idx < size; ++idx)
        {
            const VkImageViewCreateInfo info{
                .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .image            = mImages.at(idx),
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
            CHECK(vkCreateImageView(mDevice, &info, nullptr, &mImageViews.at(idx)));
        }
    }

    if(mCount != mCommandBuffers.size())
    {// Command Buffers
        if (!mCommandBuffers.empty())
            vkFreeCommandBuffers(
                mDevice,
                mCommandPool,
                static_cast<uint32_t>(mCommandBuffers.size()),
                mCommandBuffers.data()
            );

        mCommandBuffers.resize(mCount);

        const VkCommandBufferAllocateInfo info{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = mCommandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = mCount,
        };
        CHECK(vkAllocateCommandBuffers(mDevice, &info, mCommandBuffers.data()));
    }
    if(mCount != mWaitFences.size())
    {// Fences
        for (VkFence& fence : mWaitFences)
            vkDestroyFence(mDevice, fence, nullptr);
        mWaitFences.resize(mCount);

        const VkFenceCreateInfo info{
            .sType              = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext              = nullptr,
            .flags              = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        for(VkFence& fence : mWaitFences)
            CHECK(vkCreateFence(mDevice, &info, nullptr, &fence));
    }
    mResolution = next_resolution;
}

void VulkanPresentation::submit(std::uint32_t idx)
{
    constexpr const VkPipelineStageFlags kFixedWaitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkCommandBuffer& cmdbuffer = mCommandBuffers.at(idx);
    const VkSubmitInfo info{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                = nullptr,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &mSemaphorePresentComplete,
        .pWaitDstStageMask    = &kFixedWaitDstStageMask,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmdbuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &mSemaphoreRenderComplete,
    };
    CHECK(vkQueueSubmit(mQueue, 1, &info, VK_NULL_HANDLE));
}
