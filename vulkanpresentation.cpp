#include <vulkan/vulkan.h>

#include <span>
#include <vector>
#include <iterator>
#include <iostream>

#include "./vulkandebug.hpp"
#include "./vulkanutilities.hpp"

#include "./vulkanpresentation.hpp"

std::tuple<VkPhysicalDevice, std::uint32_t, std::uint32_t> VulkanPresentation::requirements(const std::span<VkPhysicalDevice>& vkphysicaldevices, VkSurfaceKHR vksurface)
{
    constexpr const std::uint32_t queue_count = 1;
    for (const VkPhysicalDevice& vkphysicaldevice : vkphysicaldevices) {
        std::uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
        assert(count > 0);

        std::vector<VkQueueFamilyProperties> queue_families_properties(count);
        vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, queue_families_properties.data());

        auto optional_queue_family_index = select_queue_family_index(queue_families_properties, VK_QUEUE_GRAPHICS_BIT);

        for (std::uint32_t idx = 0; idx < count; ++idx)
        {

            VkBool32 supported;
            CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vkphysicaldevice, idx, vksurface, &supported));

            const VkQueueFamilyProperties& p = queue_families_properties[idx];
            if((p.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (supported == VK_TRUE))
                return std::make_tuple(vkphysicaldevice, idx, queue_count);
        }
    }
    // TODO Pick the first supported if there is none with graphic support
    std::cerr << "No valid physical device supports both Graphics & Surface" << std::endl;
    return std::make_tuple(VkPhysicalDevice{VK_NULL_HANDLE}, 0u, 0u);
}

VulkanPresentation::VulkanPresentation(
        VkPhysicalDevice vkphysicaldevice,
        std::uint32_t queue_family_index,
        std::uint32_t queue_count,
        VkSurfaceKHR vksurface,
        VkExtent2D resolution,
        bool vsync
    )
    : mPhysicalDevice(vkphysicaldevice)
    , mSurface(vksurface)
    , mQueueFamilyIndex(queue_family_index)
    , mResolution(resolution)
{
    vkGetPhysicalDeviceFeatures(vkphysicaldevice, &mFeatures);
    vkGetPhysicalDeviceProperties(vkphysicaldevice, &mProperties);
    vkGetPhysicalDeviceMemoryProperties(vkphysicaldevice, &mMemoryProperties);
    {// Queue Family Properties
        std::uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
        assert(count > 0);
        mQueueFamiliesProperties.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, mQueueFamiliesProperties.data());
    }
    {// Extensions
        std::uint32_t count;
        CHECK(vkEnumerateDeviceExtensionProperties(vkphysicaldevice, nullptr, &count, nullptr));
        mExtensions.resize(count);
        CHECK(vkEnumerateDeviceExtensionProperties(vkphysicaldevice, nullptr, &count, mExtensions.data()));
    }

    std::cout << "VulkanPresentation - GPU: " << DeviceType2Text(mProperties.deviceType) << std::endl;
    {// Version
        std::uint32_t major = VK_VERSION_MAJOR(mProperties.apiVersion);
        std::uint32_t minor = VK_VERSION_MINOR(mProperties.apiVersion);
        std::uint32_t patch = VK_VERSION_PATCH(mProperties.apiVersion);
        std::cout << mProperties.deviceName << ": " << major << "." << minor << "." << patch << std::endl;
        major = VK_VERSION_MAJOR(mProperties.driverVersion);
        minor = VK_VERSION_MINOR(mProperties.driverVersion);
        patch = VK_VERSION_PATCH(mProperties.driverVersion);
        std::cout << "Driver: " << major << "." << minor << "." << patch << std::endl;
    }

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
            // TODO Check the specification what is the right minimal requirement in this situation
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
    {// Device
        const std::vector<float> priorities(queue_count, 0.0f);
        const VkDeviceQueueCreateInfo info_queue{
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .queueFamilyIndex = mQueueFamilyIndex,
            .queueCount       = queue_count,
            .pQueuePriorities = priorities.data(),
        };
        constexpr const VkPhysicalDeviceFeatures features{
            .robustBufferAccess                      = VK_FALSE,
            .fullDrawIndexUint32                     = VK_FALSE,
            .imageCubeArray                          = VK_FALSE,
            .independentBlend                        = VK_FALSE,
            .geometryShader                          = VK_FALSE,
            .tessellationShader                      = VK_FALSE,
            .sampleRateShading                       = VK_FALSE,
            .dualSrcBlend                            = VK_FALSE,
            .logicOp                                 = VK_FALSE,
            .multiDrawIndirect                       = VK_FALSE,
            .drawIndirectFirstInstance               = VK_FALSE,
            .depthClamp                              = VK_FALSE,
            .depthBiasClamp                          = VK_FALSE,
            .fillModeNonSolid                        = VK_FALSE,
            .depthBounds                             = VK_FALSE,
            .wideLines                               = VK_FALSE,
            .largePoints                             = VK_FALSE,
            .alphaToOne                              = VK_FALSE,
            .multiViewport                           = VK_FALSE,
            .samplerAnisotropy                       = VK_FALSE,
            .textureCompressionETC2                  = VK_FALSE,
            .textureCompressionASTC_LDR              = VK_FALSE,
            .textureCompressionBC                    = VK_FALSE,
            .occlusionQueryPrecise                   = VK_FALSE,
            .pipelineStatisticsQuery                 = VK_FALSE,
            .vertexPipelineStoresAndAtomics          = VK_FALSE,
            .fragmentStoresAndAtomics                = VK_FALSE,
            .shaderTessellationAndGeometryPointSize  = VK_FALSE,
            .shaderImageGatherExtended               = VK_FALSE,
            .shaderStorageImageExtendedFormats       = VK_FALSE,
            .shaderStorageImageMultisample           = VK_FALSE,
            .shaderStorageImageReadWithoutFormat     = VK_FALSE,
            .shaderStorageImageWriteWithoutFormat    = VK_FALSE,
            .shaderUniformBufferArrayDynamicIndexing = VK_FALSE,
            .shaderSampledImageArrayDynamicIndexing  = VK_FALSE,
            .shaderStorageBufferArrayDynamicIndexing = VK_FALSE,
            .shaderStorageImageArrayDynamicIndexing  = VK_FALSE,
            .shaderClipDistance                      = VK_FALSE,
            .shaderCullDistance                      = VK_FALSE,
            .shaderFloat64                           = VK_FALSE,
            .shaderInt64                             = VK_FALSE,
            .shaderInt16                             = VK_FALSE,
            .shaderResourceResidency                 = VK_FALSE,
            .shaderResourceMinLod                    = VK_FALSE,
            .sparseBinding                           = VK_FALSE,
            .sparseResidencyBuffer                   = VK_FALSE,
            .sparseResidencyImage2D                  = VK_FALSE,
            .sparseResidencyImage3D                  = VK_FALSE,
            .sparseResidency2Samples                 = VK_FALSE,
            .sparseResidency4Samples                 = VK_FALSE,
            .sparseResidency8Samples                 = VK_FALSE,
            .sparseResidency16Samples                = VK_FALSE,
            .sparseResidencyAliased                  = VK_FALSE,
            .variableMultisampleRate                 = VK_FALSE,
            .inheritedQueries                        = VK_FALSE,
        };

        std::vector<const char*> enabled_extensions{};
        if (has_extension(mExtensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
        {// Debug Marker
            enabled_extensions.emplace_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        }
        const VkDeviceCreateInfo info{
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .queueCreateInfoCount    = 1,
            .pQueueCreateInfos       = &info_queue,
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            .enabledExtensionCount   = static_cast<std::uint32_t>(enabled_extensions.size()),
            .ppEnabledExtensionNames = enabled_extensions.data(),
            .pEnabledFeatures        = &features,
        };
        CHECK(vkCreateDevice(mPhysicalDevice, &info, nullptr, &mDevice));
    }
    vkGetDeviceQueue(mDevice, mQueueFamilyIndex, 0, &mQueue);

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
