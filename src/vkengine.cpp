#include "./vkengine.hpp"

#include "./vkutilities.hpp"
#include "./vkdebug.hpp"
#include "./vkqueue.hpp"
#include "./vkphysicaldevice.hpp"

#include <vulkan/vulkan_core.h>

#include <utilities.hpp>

#include <string.h>

#include <cassert>
#include <cstddef>
#include <cinttypes>

#include <chrono>
#include <limits>
#include <ranges>
#include <numeric>
#include <iterator>

#include <array>
#include <vector>
#include <variant>
#include <unordered_map>

namespace
{
    constexpr bool kVSync = true;

    constexpr std::uint32_t kSubpassUI = 0;
    constexpr std::uint32_t kSubpassScene = 1;

    constexpr std::uint32_t kAttachmentColor = 0;
    constexpr std::uint32_t kAttachmentDepth = 1;

    // TODO(andrea.machizaud) use literals...
    // TODO(andrea.machizaud) pre-allocate a reasonable amount for buffers
    constexpr std::size_t kStagingBufferSize = 2 << 20; // 1 Mb

    constexpr VkPhysicalDeviceFeatures kFeatures{
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

    constexpr VkPhysicalDeviceVulkan12Features kVK12Features{
        .sType                                              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext                                              = nullptr,
        .samplerMirrorClampToEdge                           = VK_FALSE,
        .drawIndirectCount                                  = VK_FALSE,
        .storageBuffer8BitAccess                            = VK_FALSE,
        .uniformAndStorageBuffer8BitAccess                  = VK_FALSE,
        .storagePushConstant8                               = VK_FALSE,
        .shaderBufferInt64Atomics                           = VK_FALSE,
        .shaderSharedInt64Atomics                           = VK_FALSE,
        .shaderFloat16                                      = VK_FALSE,
        .shaderInt8                                         = VK_FALSE,
        .descriptorIndexing                                 = VK_FALSE,
        .shaderInputAttachmentArrayDynamicIndexing          = VK_FALSE,
        .shaderUniformTexelBufferArrayDynamicIndexing       = VK_FALSE,
        .shaderStorageTexelBufferArrayDynamicIndexing       = VK_FALSE,
        .shaderUniformBufferArrayNonUniformIndexing         = VK_FALSE,
        .shaderSampledImageArrayNonUniformIndexing          = VK_FALSE,
        .shaderStorageBufferArrayNonUniformIndexing         = VK_FALSE,
        .shaderStorageImageArrayNonUniformIndexing          = VK_FALSE,
        .shaderInputAttachmentArrayNonUniformIndexing       = VK_FALSE,
        .shaderUniformTexelBufferArrayNonUniformIndexing    = VK_FALSE,
        .shaderStorageTexelBufferArrayNonUniformIndexing    = VK_FALSE,
        .descriptorBindingUniformBufferUpdateAfterBind      = VK_FALSE,
        .descriptorBindingSampledImageUpdateAfterBind       = VK_FALSE,
        .descriptorBindingStorageImageUpdateAfterBind       = VK_FALSE,
        .descriptorBindingStorageBufferUpdateAfterBind      = VK_FALSE,
        .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE,
        .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE,
        .descriptorBindingUpdateUnusedWhilePending          = VK_FALSE,
        .descriptorBindingPartiallyBound                    = VK_FALSE,
        .descriptorBindingVariableDescriptorCount           = VK_FALSE,
        .runtimeDescriptorArray                             = VK_FALSE,
        .samplerFilterMinmax                                = VK_FALSE,
        .scalarBlockLayout                                  = VK_FALSE,
        .imagelessFramebuffer                               = VK_FALSE,
        .uniformBufferStandardLayout                        = VK_FALSE,
        .shaderSubgroupExtendedTypes                        = VK_FALSE,
        .separateDepthStencilLayouts                        = VK_TRUE,
        .hostQueryReset                                     = VK_FALSE,
        .timelineSemaphore                                  = VK_FALSE,
        .bufferDeviceAddress                                = VK_FALSE,
        .bufferDeviceAddressCaptureReplay                   = VK_FALSE,
        .bufferDeviceAddressMultiDevice                     = VK_FALSE,
        .vulkanMemoryModel                                  = VK_FALSE,
        .vulkanMemoryModelDeviceScope                       = VK_FALSE,
        .vulkanMemoryModelAvailabilityVisibilityChains      = VK_FALSE,
        .shaderOutputViewportIndex                          = VK_FALSE,
        .shaderOutputLayer                                  = VK_FALSE,
    };

    constexpr std::array kEnabledExtensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
}

namespace blk
{

Engine::Engine(
    VkInstance vkinstance,
    VkSurfaceKHR vksurface,
    const blk::PhysicalDevice& vkphysicaldevice,
    const std::span<VkDeviceQueueCreateInfo>& info_queues,
    const VkExtent2D& resolution)
    : mInstance(vkinstance)
    , mSurface(vksurface)
    , mPhysicalDevice(vkphysicaldevice)
    , mResolution(resolution)
    , mDevice(mPhysicalDevice, VkDeviceCreateInfo{
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &kVK12Features,
        .flags                   = 0,
        .queueCreateInfoCount    = static_cast<std::uint32_t>(info_queues.size()),
        .pQueueCreateInfos       = info_queues.data(),
        .enabledExtensionCount   = kEnabledExtensions.size(),
        .ppEnabledExtensionNames = kEnabledExtensions.data(),
        .pEnabledFeatures        = &kFeatures,
    })
    , mStagingBuffer(kStagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
{
    {// Device
        mDevice.create();
        const std::size_t queue_count = std::accumulate(
            std::begin(info_queues), std::end(info_queues),
            std::size_t{0},
            [](std::size_t current, const VkDeviceQueueCreateInfo& info) {
                return current + info.queueCount;
            }
        );

        // NOTE This is *required* so that later `emplace_back` dos not invalidate any references
        mQueues.reserve(queue_count);
        mPresentationQueues.reserve(queue_count);
        mGraphicsQueues.reserve(queue_count);
        mComputeQueues.reserve(queue_count);
        mSparseQueues.reserve(queue_count);
        mTransferQueues.reserve(queue_count);

        for (auto&& info : info_queues)
        {
            auto finder = std::ranges::find(
                mPhysicalDevice.mQueueFamilies,
                info.queueFamilyIndex,
                &blk::QueueFamily::mIndex
            );
            assert(finder != std::ranges::end(mPhysicalDevice.mQueueFamilies));

            const blk::QueueFamily& family = *finder;

            const bool sparse       = family.mProperties.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT;
            const bool compute      = family.mProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;
            const bool transfer     = family.mProperties.queueFlags & VK_QUEUE_TRANSFER_BIT;
            const bool graphics     = family.mProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
            const bool presentation = graphics && family.supports_presentation() && family.supports_surface(vksurface);

            for (std::uint32_t idx_queue = 0; idx_queue < info.queueCount; ++idx_queue)
            {
                VkQueue vkqueue = VK_NULL_HANDLE;
                vkGetDeviceQueue(mDevice, family.mIndex, idx_queue, &vkqueue);
                blk::Queue& queue = mQueues.emplace_back(family, vkqueue, idx_queue);

                // NOTE We store queue based on the most specific job it can achieve (except presentation)
                if (presentation)
                {
                    mPresentationQueues.push_back(&queue);
                }
                if (graphics)
                {
                    mGraphicsQueues.push_back(&queue);
                }
                else if (compute)
                {
                    mComputeQueues.push_back(&queue);
                }
                else
                {
                    if (sparse)
                        mSparseQueues.push_back(&queue);
                    if (transfer)
                        mTransferQueues.push_back(&queue);
                }
            }
        }

        mPresentationQueues.shrink_to_fit();
        mGraphicsQueues.shrink_to_fit();
        mComputeQueues.shrink_to_fit();
        mSparseQueues.shrink_to_fit();
        mTransferQueues.shrink_to_fit();
    }
    {// Pipeline Cache
        const VkPipelineCacheCreateInfo info{
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0u,
            // TODO
            .initialDataSize = 0u,
            .pInitialData    = nullptr,
        };
        CHECK(vkCreatePipelineCache(mDevice, &info, nullptr, &mPipelineCache));
    }
    {// Pools
        assert(!mTransferQueues.empty());
        assert(!mPresentationQueues.empty());
        const blk::Queue* transfer_queue = mTransferQueues.at(0);
        const blk::Queue* presentation_queue = mPresentationQueues.at(0);
        {// Command Pools
            const VkCommandPoolCreateInfo info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = presentation_queue->mFamily.mIndex,
            };
            CHECK(vkCreateCommandPool(mDevice, &info, nullptr, &mPresentationCommandPool));
        }
        {// Command Pools - One Off
            const VkCommandPoolCreateInfo info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                .queueFamilyIndex = transfer_queue->mFamily.mIndex,
            };
            CHECK(vkCreateCommandPool(mDevice, &info, nullptr, &mTransferCommandPool));
        }
    }
    {// Staging
        {// Fences
            const VkFenceCreateInfo info{
                .sType              = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext              = nullptr,
                .flags              = 0,
            };
            CHECK(vkCreateFence(mDevice, &info, nullptr, &mStagingFence));
        }
        {// Command Buffers
            const VkCommandBufferAllocateInfo info{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext              = nullptr,
                .commandPool        = mTransferCommandPool,
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            CHECK(vkAllocateCommandBuffers(mDevice, &info, &mStagingCommandBuffer));
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
            CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mStagingSemaphore));
        }
        // Buffer
        mStagingBuffer.create(mDevice);
    }
}

Engine::~Engine()
{
    for(auto&& vkframebuffer : mFrameBuffers)
        vkDestroyFramebuffer(mDevice, vkframebuffer, nullptr);

    for(auto&& view : mPresentation.views)
        vkDestroyImageView(mDevice, view, nullptr);
    vkDestroySwapchainKHR(mDevice, mPresentation.swapchain, nullptr);

    vkDestroySemaphore(mDevice, mRenderSemaphore, nullptr);
    vkDestroySemaphore(mDevice, mAcquiredSemaphore, nullptr);

    vkDestroySemaphore(mDevice, mStagingSemaphore, nullptr);
    vkDestroyFence(mDevice, mStagingFence, nullptr);

    vkDestroyCommandPool(mDevice, mPresentationCommandPool, nullptr);
    vkDestroyCommandPool(mDevice, mGraphicsCommandPool, nullptr);
    vkDestroyCommandPool(mDevice, mTransferCommandPool, nullptr);
    vkDestroyCommandPool(mDevice, mComputeCommandPool, nullptr);

    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);

    vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
}

void Engine::initialize()
{
    {// Present Modes
        mPresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
        if(kVSync)
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

            constexpr std::array kPreferredPresentModes{
                VK_PRESENT_MODE_MAILBOX_KHR,
                VK_PRESENT_MODE_IMMEDIATE_KHR,
                VK_PRESENT_MODE_FIFO_KHR,
            };

            for (auto&& preferred : kPreferredPresentModes)
            {
                auto finder_present_mode = std::find(
                    std::begin(present_modes), std::end(present_modes),
                    preferred
                );

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
            constexpr VkFormat kPreferredFormat = VK_FORMAT_B8G8R8A8_UNORM;
            if((count == 1) && (formats.front().format == VK_FORMAT_UNDEFINED))
            {
                // NOTE No preferred format, pick what you want
                mColorAttachmentFormat = kPreferredFormat;
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
                    mColorAttachmentFormat = preferred_format.format;
                    mColorSpace  = preferred_format.colorSpace;
                }
                else
                {
                    mColorAttachmentFormat = formats.front().format;
                    mColorSpace  = formats.front().colorSpace;
                }
            }
        }
        {// Depth/Stencil Attachment
            // NVIDIA - https://developer.nvidia.com/blog/vulkan-dos-donts/
            //  > Prefer using 24 bit depth formats for optimal performance
            //  > Prefer using packed depth/stencil formats. This is a common cause for notable performance differences between an OpenGL and Vulkan implementation.
            constexpr VkFormat kDEPTH_FORMATS[] = {
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D16_UNORM,
                VK_FORMAT_D32_SFLOAT,
            };

            auto finder = std::find_if(
                std::begin(kDEPTH_FORMATS), std::end(kDEPTH_FORMATS),
                [vkphysicaldevice = mPhysicalDevice](const VkFormat& f) {
                    VkFormatProperties properties;
                    vkGetPhysicalDeviceFormatProperties(vkphysicaldevice, f, &properties);
                    return properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
                }
            );

            assert(finder != std::end(kDEPTH_FORMATS));
            mDepthStencilAttachmentFormat = *finder;
        }
    }
    {// Render Pass
        // Pass 0 : Draw UI    (write depth)
        // Pass 1 : Draw Scene (read depth, write color)
        //  Depth discarded
        //  Color kept
        //  Transition to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR to optimize transition before presentation
        const std::array attachments{
            VkAttachmentDescription{
                .flags          = 0,
                .format         = mColorAttachmentFormat,
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            VkAttachmentDescription{
                .flags          = 0,
                .format         = mDepthStencilAttachmentFormat,
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            },
        };
        constexpr VkAttachmentReference write_color_reference{
            .attachment = kAttachmentColor,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        constexpr VkAttachmentReference write_stencil_reference{
            .attachment = kAttachmentDepth,
            .layout     = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
        };
        constexpr VkAttachmentReference read_stencil_reference{
            .attachment = kAttachmentDepth,
            .layout     = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
        };
        // C:/devel/vkplaygrounds/src/dearimgui/Engine.cpp:514:30: error: constexpr variable 'subpasses' must be initialized by a constant expression
        //         constexpr std::array subpasses{
        //                              ^        ~
        // C:/devel/vkplaygrounds/src/dearimgui/Engine.cpp:514:30: note: pointer to 'write_color_reference' is not a constant expression
        // C:/devel/vkplaygrounds/src/dearimgui/Engine.cpp:502:47: note: declared here
        //         constexpr const VkAttachmentReference write_color_reference{
        //                                               ^
        static const/*expr*/ std::array subpasses{
            // Pass 0 : Draw UI    (write stencil, write color)
            VkSubpassDescription{
                .flags                   = 0,
                .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount    = 0,
                .pInputAttachments       = nullptr,
                .colorAttachmentCount    = 1,
                .pColorAttachments       = &write_color_reference,
                .pResolveAttachments     = nullptr,
                .pDepthStencilAttachment = &write_stencil_reference,
                .preserveAttachmentCount = 0,
                .pPreserveAttachments    = nullptr,
            },
            // Pass 1 : Draw Scene (read stencil, write color)
            VkSubpassDescription{
                .flags                   = 0,
                .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount    = 0,
                .pInputAttachments       = nullptr,
                .colorAttachmentCount    = 1,
                .pColorAttachments       = &write_color_reference,
                .pResolveAttachments     = nullptr,
                .pDepthStencilAttachment = &read_stencil_reference,
                .preserveAttachmentCount = 0,
                .pPreserveAttachments    = nullptr,
            },
        };
        constexpr std::array dependencies{
            VkSubpassDependency{
                .srcSubpass      = kSubpassUI,
                .dstSubpass      = kSubpassScene,
                .srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT ,
                .dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT ,
                .srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
        };
        const VkRenderPassCreateInfo info{
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .attachmentCount = attachments.size(),
            .pAttachments    = attachments.data(),
            .subpassCount    = subpasses.size(),
            .pSubpasses      = subpasses.data(),
            .dependencyCount = dependencies.size(),
            .pDependencies   = dependencies.data(),
        };
        CHECK(vkCreateRenderPass(mDevice, &info, nullptr, &mRenderPass));
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
        // FIXME Delegate to presentation
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mAcquiredSemaphore));
        // FIXME Delegate to presentation
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mRenderSemaphore));
    }
    initialize_resources();
}

void Engine::initialize_resources()
{
    // NVIDIA - https://developer.nvidia.com/blog/vulkan-dos-donts/
    //  > Parallelize command buffer recording, image and buffer creation, descriptor set updates, pipeline creation, and memory allocation / binding. Task graph architecture is a good option which allows sufficient parallelism in terms of draw submission while also respecting resource and command queue dependencies.
    {// Images
        {// Depth
            mDepthImage = blk::Image(
                VkExtent3D{ .width = mResolution.width, .height = mResolution.height, .depth = 1 },
                VK_IMAGE_TYPE_2D,
                mDepthStencilAttachmentFormat,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED
            );
            mDepthImage.create(mDevice);
        }
    }
}

void Engine::initialize_views()
{
    mDepthImageView = blk::ImageView(
        mDepthImage,
        VK_IMAGE_VIEW_TYPE_2D,
        mDepthStencilAttachmentFormat,
        mDepthStencilAttachmentFormat >= VK_FORMAT_D16_UNORM_S8_UINT
            ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
            : VK_IMAGE_ASPECT_DEPTH_BIT
    );
    mDepthImageView.create(mDevice);
}

void Engine::allocate_memory_and_bind_resources(
    const std::span<std::tuple<blk::Buffer*, VkMemoryPropertyFlags>>& buffers,
    const std::span<std::tuple<blk::Image*, VkMemoryPropertyFlags>>& images)
{
    struct Resources
    {
        std::vector<blk::Image*> images;
        std::vector<blk::Buffer*> buffers;
    };

    // group resource by memory type
    std::unordered_map<const blk::MemoryType*, Resources> resources_by_types;

    for (auto&& [resource, flags] : images)
    {
        auto memory_type = mPhysicalDevice.mMemories.find_compatible(*resource, flags);
        assert(memory_type);
        resources_by_types[memory_type].images.push_back(resource);
    }
    for (auto&& [resource, flags] : buffers)
    {
        auto memory_type = mPhysicalDevice.mMemories.find_compatible(*resource, flags);
        assert(memory_type);
        resources_by_types[memory_type].buffers.push_back(resource);
    }

    assert(mMemoryChunks.empty());
    mMemoryChunks.reserve(resources_by_types.size());
    for(auto&& entry : resources_by_types)
    {
        const VkDeviceSize required_size_images = std::/*ranges::*/accumulate(
            std::begin(entry.second.images), std::end(entry.second.images),
            VkDeviceSize{0},
            [](VkDeviceSize size, const blk::Image* image) -> VkDeviceSize{
                return size + image->mRequirements.size;
            }
        );
        const VkDeviceSize required_size_buffers = std::/*ranges::*/accumulate(
            std::begin(entry.second.buffers), std::end(entry.second.buffers),
            VkDeviceSize{0},
            [](VkDeviceSize size, const blk::Buffer* buffer) -> VkDeviceSize{
                return size + buffer->mRequirements.size;
            }
        );

        blk::Memory& chunk = mMemoryChunks.emplace_back(*entry.first, required_size_images + required_size_buffers);
        chunk.allocate(mDevice);
        if (!entry.second.images.empty())
            chunk.bind(entry.second.images);
        if (!entry.second.buffers.empty())
            chunk.bind(entry.second.buffers);
    }
}

void Engine::create_swapchain()
{
    VkSurfaceCapabilitiesKHR capabilities;
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &capabilities));

    mResolution = (capabilities.currentExtent.width == std::numeric_limits<std::uint32_t>::max())
        ? mResolution
        : capabilities.currentExtent;

    {// Creation
        VkSwapchainKHR previous_swapchain = mPresentation.swapchain;

        const std::uint32_t image_count = (capabilities.maxImageCount > 0)
            ? std::min(capabilities.minImageCount + 1, capabilities.maxImageCount)
            : capabilities.minImageCount + 1;

        const VkSurfaceTransformFlagBitsKHR transform = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
            : capabilities.currentTransform;

        constexpr VkCompositeAlphaFlagBitsKHR kPreferredCompositeAlphaFlags[] = {
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
            .imageFormat           = mColorAttachmentFormat,
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
        CHECK(vkCreateSwapchainKHR(mDevice, &info, nullptr, &(mPresentation.swapchain)));

        if (previous_swapchain != VK_NULL_HANDLE)
        {// Cleaning
            for(auto&& view : mPresentation.views)
                vkDestroyImageView(mDevice, view, nullptr);
            vkDestroySwapchainKHR(mDevice, previous_swapchain, nullptr);
        }
    }
    {// Views
        std::uint32_t count;
        CHECK(vkGetSwapchainImagesKHR(mDevice, mPresentation.swapchain, &count, nullptr));
        mPresentation.images.resize(count);
        mPresentation.views.resize(count);
        CHECK(vkGetSwapchainImagesKHR(mDevice, mPresentation.swapchain, &count, mPresentation.images.data()));

        for (auto&& [image, view] : zip(mPresentation.images, mPresentation.views))
        {
            const VkImageViewCreateInfo info{
                .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .image            = image,
                .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                .format           = mColorAttachmentFormat,
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
            CHECK(vkCreateImageView(mDevice, &info, nullptr, &view));
        }
    }
}

void Engine::create_framebuffers()
{
    mFrameBuffers.resize(mPresentation.views.size());
    for (auto&& [vkview, vkframebuffer] : zip(mPresentation.views, mFrameBuffers))
    {
        const std::array attachments{
            vkview,
            // NOTE(andrea.machizaud) Is it safe to re-use depth here ?
            (VkImageView)mDepthImageView
        };
        const VkFramebufferCreateInfo info{
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0u,
            .renderPass      = mRenderPass,
            .attachmentCount = attachments.size(),
            .pAttachments    = attachments.data(),
            .width           = mResolution.width,
            .height          = mResolution.height,
            .layers          = 1,
        };
        CHECK(vkCreateFramebuffer(mDevice, &info, nullptr, &vkframebuffer));
    }
}

void Engine::create_commandbuffers()
{
    mCommandBuffers.resize(mPresentation.views.size());
    const VkCommandBufferAllocateInfo info{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = mPresentationCommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<std::uint32_t>(mCommandBuffers.size()),
    };
    CHECK(vkAllocateCommandBuffers(mDevice, &info, mCommandBuffers.data()));
}

AcquiredPresentationImage Engine::acquire()
{
    std::uint32_t index = ~0;
    const VkResult status = vkAcquireNextImageKHR(
        mDevice,
        mPresentation.swapchain,
        std::numeric_limits<std::uint64_t>::max(),
        mAcquiredSemaphore,
        VK_NULL_HANDLE,
        &index
    );
    CHECK(status);
    return AcquiredPresentationImage{ index, mCommandBuffers.at(index) };
}

void Engine::present(const AcquiredPresentationImage& presentationimage)
{
    const VkPresentInfoKHR info{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext              = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &mRenderSemaphore,
        .swapchainCount     = 1,
        .pSwapchains        = &mPresentation.swapchain,
        .pImageIndices      = &presentationimage.index,
        .pResults           = nullptr,
    };
    const blk::Queue* presentation_queue = mPresentationQueues.at(0);
    const VkResult result_present = vkQueuePresentKHR(*presentation_queue, &info);
    CHECK(result_present);
}

#if 0
void Engine::record(AcquiredPresentationImage& presentationimage)
{
    // FIXME RenderPass abstraction required

    VkCommandBuffer cmdbuffer = presentationimage.commandbuffer;

    {// Begin
        constexpr VkCommandBufferBeginInfo info{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .pInheritanceInfo = nullptr,
        };
        CHECK(vkBeginCommandBuffer(cmdbuffer, &info));
    }
    {// Render Passes
        { // Subpass 0 ; ImGui
            constexpr std::array kClearValues {
                VkClearValue {
                    .color = VkClearColorValue{
                        .float32 = { 0.2f, 0.2f, 0.2f, 1.0f }
                    },
                },
                VkClearValue {
                    .depthStencil = VkClearDepthStencilValue{
                        .depth   = 0.0f,
                        .stencil = 0,
                    }
                }
            };
            const VkRenderPassBeginInfo info{
                .sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .pNext            = nullptr,
                .renderPass       = mRenderPass,
                .framebuffer      = mFrameBuffers.at(presentationimage.index),
                .renderArea       = VkRect2D{
                    .offset = VkOffset2D{
                        .x = 0,
                        .y = 0,
                    },
                    .extent = mResolution
                },
                .clearValueCount  = kClearValues.size(),
                .pClearValues     = kClearValues.data(),
            };
            vkCmdBeginRenderPass(cmdbuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines.at(0));
            vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mUIPipelineLayout,
                0,
                1, &mDescriptorSet,
                0, nullptr
            );
            // What should we do ?!
            if (resolution.width != 0 && resolution.height != 0)
            {
                assert(viewport.width > 0);
                assert(viewport.height > 0);
                vkCmdSetViewport(cmdbuffer, 0, 1, &viewport);
            }
            {// Constants
                DearImGuiConstants constants{};
                constants.scale    [0] =  2.0f / data->DisplaySize.x;
                constants.scale    [1] =  2.0f / data->DisplaySize.y;
                constants.translate[0] = -1.0f - data->DisplayPos.x * constants.scale[0];
                constants.translate[1] = -1.0f - data->DisplayPos.y * constants.scale[1];
                vkCmdPushConstants(cmdbuffer, mUIPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);
            }
            if (data->TotalVtxCount > 0)
            {// Buffer Bindings
                constexpr VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(cmdbuffer, kVertexInputBindingPosUVColor, 1, &mVertexBuffer.mBuffer, &offset);
                vkCmdBindIndexBuffer(cmdbuffer, mIndexBuffer, offset, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
            }
            {// Draws
                // Utilities to project scissor/clipping rectangles into framebuffer space
                const ImVec2 clip_offset = data->DisplayPos;        // (0,0) unless using multi-viewports
                const ImVec2 clip_scale  = data->FramebufferScale;  // (1,1) unless using retina display which are often (2,2)

                std::uint32_t offset_index = 0;
                std::int32_t  offset_vertex = 0;
                for (auto idx_list = 0, count_list = data->CmdListsCount; idx_list < count_list; ++idx_list)
                {
                    const ImDrawList* list = data->CmdLists[idx_list];
                    for (auto idx_buffer = 0, count_buffer = list->CmdBuffer.Size; idx_buffer < count_buffer; ++idx_buffer)
                    {
                        const ImDrawCmd& command = list->CmdBuffer[idx_buffer];
                        assert(command.UserCallback == nullptr);

                        // Clip Rect in framebuffer space
                        const ImVec4 framebuffer_clip_rectangle(
                            (command.ClipRect.x - clip_offset.x) * clip_scale.x,
                            (command.ClipRect.y - clip_offset.y) * clip_scale.y,
                            (command.ClipRect.z - clip_offset.x) * clip_scale.x,
                            (command.ClipRect.w - clip_offset.y) * clip_scale.y
                        );

                        const bool valid_clip(
                            (framebuffer_clip_rectangle.x < viewport.width ) &&
                            (framebuffer_clip_rectangle.y < viewport.height) &&
                            (framebuffer_clip_rectangle.w >= 0.0           ) &&
                            (framebuffer_clip_rectangle.w >= 0.0           )
                        );
                        if (valid_clip)
                        {
                            const VkRect2D scissors{
                                .offset = VkOffset2D{
                                    .x = std::max(static_cast<std::int32_t>(framebuffer_clip_rectangle.x), 0),
                                    .y = std::max(static_cast<std::int32_t>(framebuffer_clip_rectangle.y), 0),
                                },
                                .extent = VkExtent2D{
                                    .width  = static_cast<std::uint32_t>(framebuffer_clip_rectangle.z - framebuffer_clip_rectangle.x),
                                    .height = static_cast<std::uint32_t>(framebuffer_clip_rectangle.w - framebuffer_clip_rectangle.y),
                                }
                            };

                            vkCmdSetScissor(cmdbuffer, 0, 1, &scissors);
                            vkCmdDrawIndexed(cmdbuffer, command.ElemCount, 1, command.IdxOffset + offset_index, command.VtxOffset + offset_vertex, 0);
                        }
                    }
                    offset_index  += list->IdxBuffer.Size;
                    offset_vertex += list->VtxBuffer.Size;
                }
            }
        }
        {// Subpass 1 - Scene
            vkCmdNextSubpass(cmdbuffer, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines.at(1));
            vkCmdDraw(cmdbuffer, 3, 1, 0, 0);
        }
        vkCmdEndRenderPass(cmdbuffer);
    }
    CHECK(vkEndCommandBuffer(cmdbuffer));
}

void Engine::render_frame()
{
    auto tick_start = std::chrono::high_resolution_clock::now();

    const blk::Queue* presentation_queue = mPresentationQueues.at(0);
    auto presentationimage = acquire();
    record(presentationimage);
    {// Submit
        constexpr VkPipelineStageFlags kFixedWaitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        const std::array infos{
            VkSubmitInfo{
                .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext                = nullptr,
                .waitSemaphoreCount   = 1,
                .pWaitSemaphores      = &mAcquiredSemaphore,
                .pWaitDstStageMask    = &kFixedWaitDstStageMask,
                .commandBufferCount   = 1,
                .pCommandBuffers      = &presentationimage.commandbuffer,
                .signalSemaphoreCount = 1,
                .pSignalSemaphores    = &mRenderSemaphore,
            }
        };
        CHECK(vkQueueSubmit(*presentation_queue, infos.size(), infos.data(), VK_NULL_HANDLE));
    }
    present(presentationimage);

    // NOTE We block to avoid override something in use (e.g. surface indexed VkCommandBuffer)
    // TODO Block per command buffer, instead of a global lock for all of them
    CHECK(vkQueueWaitIdle(*presentation_queue));

    auto tick_end  = std::chrono::high_resolution_clock::now();
    // mUI.frame_delta = frame_time_delta_ms_t(tick_end - tick_start);

    //++mUI.frame_count;

    //using namespace std::chrono_literals;

    // auto elapsed_time = frame_time_delta_s_t(tick_end - mUI.count_tick);
    // if (elapsed_time > 1s)
    // {
    //     auto fps = mUI.frame_count * elapsed_time;
    //     mUI.frame_count = 0;
    //     mUI.count_tick  = tick_end;
    // }
}
#endif

void Engine::wait_staging_operations()
{
    vkWaitForFences(mDevice, 1, &mStagingFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
}
}
