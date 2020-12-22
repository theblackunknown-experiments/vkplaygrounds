#include "./vkpresentation.hpp"

#include "./vkdebug.hpp"

#include "./vkqueue.hpp"
#include "./vkengine.hpp"
#include "./vksurface.hpp"
#include "./vkphysicaldevice.hpp"

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <array>
#include <vector>

#include <ranges>

namespace
{
    constexpr bool kVSync = true;

    constexpr std::array kPreferredPresentModes = {
        VK_PRESENT_MODE_MAILBOX_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_KHR,
    };

    constexpr std::array kPreferredCompositeAlphaFlags{
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };

    constexpr VkFormat kPreferredFormat = VK_FORMAT_B8G8R8A8_UNORM;
}

namespace blk
{

Presentation::Presentation(
    const blk::Engine& vkengine,
    const blk::Surface& vksurface,
    const VkExtent2D& resolution)
    : mEngine(vkengine)
    , mSurface(vksurface)
    , mResolution(resolution)

    , mDevice(vkengine.mDevice)
    , mPhysicalDevice(vkengine.mPhysicalDevice)

    , mPresentationQueues(vkengine.mPresentationQueues)
{
    {// Present Modes
        mPresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
        if (kVSync)
        {
            // guaranteed by spec
            mPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        }
        else
        {
            std::uint32_t count;
            CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &count, nullptr));
            assert(count > 0);

            std::vector<VkPresentModeKHR> present_modes(count);
            CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &count, present_modes.data()));

            for (auto&& preferred : kPreferredPresentModes)
            {
                auto finder_present_mode = std::ranges::find(present_modes, preferred);
                if (finder_present_mode != std::end(present_modes))
                {
                    mPresentMode = *finder_present_mode;
                }
            }
            assert(mPresentMode != VK_PRESENT_MODE_MAX_ENUM_KHR);
        }
    }
    {// Formats
        {// Color Attachment
            // Simple case : we target the same format as the presentation capabilities
            std::uint32_t count;
            vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &count, nullptr);
            assert(count > 0);

            std::vector<VkSurfaceFormatKHR> formats(count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &count, formats.data());

            // NOTE(andrea.machizaud) Arbitrary : it happens to be supported on my device
            if ((count == 1) && (formats.front().format == VK_FORMAT_UNDEFINED))
            {
                // NOTE No preferred format, pick what you want
                mColorFormat = kPreferredFormat;
                mColorSpace  = formats.front().colorSpace;
            }
            // otherwise looks for our preferred one, or switch back to the 1st available
            else
            {
                auto finder = std::ranges::find(formats, kPreferredFormat, &VkSurfaceFormatKHR::format);
                if (finder != std::end(formats))
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
    }
    {// Semaphores
        const VkSemaphoreTypeCreateInfo info_type{
            .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext         = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
            .initialValue  = 0,
        };
        const VkSemaphoreCreateInfo info_semaphore{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &info_type,
            .flags = 0,
        };
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mAcquiredSemaphore));
    }
    {// Pools
        assert(!mPresentationQueues.empty());
        const blk::Queue* queue = mPresentationQueues.at(0);
        {// Command Pools
            const VkCommandPoolCreateInfo info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = queue->mFamily.mIndex,
            };
            CHECK(vkCreateCommandPool(mDevice, &info, nullptr, &mCommandPool));
        }
    }
    recreate_swapchain();
}

Presentation::~Presentation()
{
    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

    vkDestroySemaphore(mDevice, mAcquiredSemaphore, nullptr);
}

VkExtent2D Presentation::recreate_swapchain()
{
    VkSurfaceCapabilitiesKHR capabilities;
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &capabilities));

    mResolution = (capabilities.currentExtent.width == std::numeric_limits<std::uint32_t>::max())
        ? mResolution
        : capabilities.currentExtent;

    {// Creation
        VkSwapchainKHR previous_swapchain = mSwapchain;

        const std::uint32_t image_count = (capabilities.maxImageCount > 0)
            ? std::min(capabilities.minImageCount + 1, capabilities.maxImageCount)
            : capabilities.minImageCount + 1;

        const VkSurfaceTransformFlagBitsKHR transform = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
            : capabilities.currentTransform;

        auto finder_composite_alpha = std::ranges::find_if(
            kPreferredCompositeAlphaFlags,
            [&capabilities](const VkCompositeAlphaFlagBitsKHR& f) {
                return capabilities.supportedCompositeAlpha & f;
            }
        );
        assert(finder_composite_alpha != std::end(kPreferredCompositeAlphaFlags));

        const VkCompositeAlphaFlagBitsKHR composite_alpha = *finder_composite_alpha;

        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // be greedy
        if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        // be greedy
        if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const VkSwapchainCreateInfoKHR info{
            .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext                 = nullptr,
            .flags                 = 0,
            .surface               = mSurface,
            .minImageCount         = image_count,
            .imageFormat           = mColorFormat,
            .imageColorSpace       = mColorSpace,
            .imageExtent           = mResolution,
            .imageArrayLayers      = 1,
            .imageUsage            = usage,
            .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .preTransform          = transform,
            .compositeAlpha        = composite_alpha,
            .presentMode           = mPresentMode,
            .clipped               = VK_TRUE,
            .oldSwapchain          = previous_swapchain,
        };
        CHECK(vkCreateSwapchainKHR(mDevice, &info, nullptr, &mSwapchain));

        if (previous_swapchain != VK_NULL_HANDLE)
        {// Cleaning
            if (!mCommandBuffers.empty())
                vkFreeCommandBuffers(
                    mDevice,
                    mCommandPool,
                    static_cast<std::uint32_t>(mCommandBuffers.size()),
                    mCommandBuffers.data()
                );
            mCommandBuffers.clear();
            vkDestroySwapchainKHR(mDevice, previous_swapchain, nullptr);
        }
    }
    {// Views
        CHECK(vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mImageCount, nullptr));
        mImages.resize(mImageCount);
        CHECK(vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mImageCount, mImages.data()));
    }
    { // Command Buffers
        mCommandBuffers.resize(mImageCount);
        const VkCommandBufferAllocateInfo info{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = mCommandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<std::uint32_t>(mCommandBuffers.size()),
        };
        CHECK(vkAllocateCommandBuffers(mDevice, &info, mCommandBuffers.data()));
    }

    return mResolution;
}

blk::Presentation::Image Presentation::acquire_next(std::uint64_t timeout)
{
    std::uint32_t index = ~0;
    const VkResult status = vkAcquireNextImageKHR(
        mDevice,
        mSwapchain,
        timeout,
        mAcquiredSemaphore,
        VK_NULL_HANDLE,
        &index
    );
    CHECK(status);
    return Image{
        index,
        mCommandBuffers.at(index),
        mAcquiredSemaphore,
        // TODO I think this depends on the actual sample (e.g. compute vs. graphics)
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
}

VkResult Presentation::present(const Image& presentation_image, VkSemaphore wait_semaphore)
{
    const VkPresentInfoKHR info{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext              = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &wait_semaphore,
        .swapchainCount     = 1,
        .pSwapchains        = &mSwapchain,
        .pImageIndices      = &presentation_image.index,
        .pResults           = nullptr,
    };
    const blk::Queue* queue = mPresentationQueues.at(0);
    const VkResult result_present = vkQueuePresentKHR(*queue, &info);
    CHECK(
        (result_present == VK_SUCCESS)
        || (result_present == VK_SUBOPTIMAL_KHR)
        || (result_present == VK_ERROR_OUT_OF_DATE_KHR)
    );
    return result_present;
}

void Presentation::onResize(const VkExtent2D& resolution)
{
    mResolution = resolution;
    recreate_swapchain();
}

}
