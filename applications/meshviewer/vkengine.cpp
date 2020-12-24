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

namespace
{
    constexpr bool kVSync = true;

    // TODO(andrea.machizaud) use literals...
    // TODO(andrea.machizaud) pre-allocate a reasonable amount for buffers
    constexpr std::size_t kStagingBufferSize = 2 << 20; // 1 Mb

    VkPhysicalDeviceFeatures kFeatures{
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

    VkPhysicalDeviceVulkan12Features kVK12Features{
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
    const blk::PhysicalDevice& vkphysicaldevice,
    const std::span<VkDeviceQueueCreateInfo>& info_queues)
    : mInstance(vkinstance)
    , mPhysicalDevice(vkphysicaldevice)
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
            const bool presentation = graphics && family.supports_presentation();

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

        // ok to invalidate : we only store pointers
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
        assert(!mGraphicsQueues.empty());
        assert(!mPresentationQueues.empty());
        const blk::Queue* transfer_queue = mTransferQueues.at(0);
        const blk::Queue* graphic_queue = mGraphicsQueues.at(0);
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
                .queueFamilyIndex = graphic_queue->mFamily.mIndex,
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

        // Memory
        auto memory_type = vkphysicaldevice.mMemories.find_compatible(mStagingBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        assert(memory_type);
        mStagingMemory = std::make_unique<blk::Memory>(*memory_type, mStagingBuffer.mRequirements.size);
        mStagingMemory->allocate(mDevice);
        mStagingMemory->bind(mStagingBuffer);
    }
}

Engine::~Engine()
{
    vkDestroySemaphore(mDevice, mStagingSemaphore, nullptr);
    vkDestroyFence(mDevice, mStagingFence, nullptr);

    vkDestroyCommandPool(mDevice, mPresentationCommandPool, nullptr);
    vkDestroyCommandPool(mDevice, mGraphicsCommandPool, nullptr);
    vkDestroyCommandPool(mDevice, mTransferCommandPool, nullptr);
    vkDestroyCommandPool(mDevice, mComputeCommandPool, nullptr);

    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
}

void Engine::submit(
        VkQueue vkqueue,
        const std::span<const VkCommandBuffer>& vkcommandbuffers,
        const std::span<const VkSemaphore>& vkwait_semaphores,
        const std::span<const VkPipelineStageFlags>& vkwait_stages,
        const std::span<const VkSemaphore>& vksignal_semaphores,
        VkFence vkfence)
{
    const std::array infos{
        VkSubmitInfo{
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = (std::uint32_t)vkwait_semaphores.size(),
            .pWaitSemaphores      = vkwait_semaphores.data(),
            .pWaitDstStageMask    = vkwait_stages.data(),
            .commandBufferCount   = (std::uint32_t)vkcommandbuffers.size(),
            .pCommandBuffers      = vkcommandbuffers.data(),
            .signalSemaphoreCount = (std::uint32_t)vksignal_semaphores.size(),
            .pSignalSemaphores    = vksignal_semaphores.data(),
        }
    };
    CHECK(vkQueueSubmit(vkqueue, infos.size(), infos.data(), vkfence));
}

void Engine::submit(
        VkQueue vkqueue,
        const std::initializer_list<VkCommandBuffer>& vkcommandbuffers,
        const std::initializer_list<VkSemaphore>& vkwait_semaphores,
        const std::initializer_list<VkPipelineStageFlags>& vkwait_stages,
        const std::initializer_list<VkSemaphore>& vksignal_semaphores,
        VkFence vkfence)
{
    std::vector<VkCommandBuffer> commandbuffers(std::begin(vkcommandbuffers), std::end(vkcommandbuffers));
    std::vector<VkSemaphore> wait_semaphores(std::begin(vkwait_semaphores), std::end(vkwait_semaphores));
    std::vector<VkPipelineStageFlags> wait_stages(std::begin(vkwait_stages), std::end(vkwait_stages));
    std::vector<VkSemaphore> signal_semaphores(std::begin(vksignal_semaphores), std::end(vksignal_semaphores));
    submit(
        vkqueue,
        std::span<VkCommandBuffer>(commandbuffers),
        std::span<VkSemaphore>(wait_semaphores),
        std::span<VkPipelineStageFlags>(wait_stages),
        std::span<VkSemaphore>(signal_semaphores),
        vkfence
    );
}

void Engine::wait_staging_operations()
{
    vkWaitForFences(mDevice, 1, &mStagingFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
}
}
