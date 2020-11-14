#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <span>
#include <tuple>
#include <memory>

#include <array>
#include <chrono>
#include <vector>

#include "./vkutilities.hpp"
#include "./vkdevice.hpp"

#include "./vkbuffer.hpp"
#include "./vkimage.hpp"

namespace blk
{

struct Queue;
struct Image;
struct Memory;
struct Buffer;
struct ImageView;
struct PhysicalDevice;

struct Engine
{
    explicit Engine(
        VkInstance vkinstance,
        const blk::PhysicalDevice& vkphysicaldevice,
        const std::span<VkDeviceQueueCreateInfo>& info_queues);
    ~Engine();

    [[deprecated("although useful for debugging, but it is recommended to manage your memory lifetime/binding yourself")]]
    void allocate_memory_and_bind_resources(
        std::span<std::tuple<blk::Buffer*, VkMemoryPropertyFlags>> buffers,
        std::span<std::tuple<blk::Image*, VkMemoryPropertyFlags>> images);

    static
    void submit(VkQueue vkqueue, VkCommandBuffer vkcommandbuffer, VkFence vkfence = VK_NULL_HANDLE);
    static
    void submit(VkQueue vkqueue, VkCommandBuffer vkcommandbuffer, VkSemaphore vkwait_semaphore, VkPipelineStageFlags vkwait_stage, VkSemaphore vksignal_semaphore, VkFence vkfence = VK_NULL_HANDLE);

    void wait_staging_operations();

    VkInstance                                mInstance                = VK_NULL_HANDLE;
    VkSurfaceKHR                              mSurface                 = VK_NULL_HANDLE;
    const blk::PhysicalDevice&                mPhysicalDevice;
     
    blk::Device                               mDevice;
     
    std::vector<blk::Queue>                   mQueues;
    std::vector<blk::Queue*>                  mSparseQueues;
    std::vector<blk::Queue*>                  mComputeQueues;
    std::vector<blk::Queue*>                  mTransferQueues;
    std::vector<blk::Queue*>                  mGraphicsQueues;
    std::vector<blk::Queue*>                  mPresentationQueues;
     
    VkPipelineCache                           mPipelineCache           = VK_NULL_HANDLE;
     
    VkCommandPool                             mComputeCommandPool      = VK_NULL_HANDLE;
    VkCommandPool                             mTransferCommandPool     = VK_NULL_HANDLE;
    VkCommandPool                             mGraphicsCommandPool     = VK_NULL_HANDLE;
    VkCommandPool                             mPresentationCommandPool = VK_NULL_HANDLE;
     
    blk::Buffer                               mStagingBuffer;
    VkFence                                   mStagingFence            = VK_NULL_HANDLE;
    VkSemaphore                               mStagingSemaphore        = VK_NULL_HANDLE;
    VkCommandBuffer                           mStagingCommandBuffer    = VK_NULL_HANDLE;
    std::unique_ptr<blk::Memory>              mStagingMemory;

    std::vector<std::unique_ptr<blk::Memory>> mMemoryChunks;
};

}

