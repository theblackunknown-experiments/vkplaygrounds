#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

namespace blk
{
    struct PhysicalDevice;

    struct QueueFamily
    {
        std::uint32_t              mIndex;
        VkQueueFamilyProperties    mProperties;
        const blk::PhysicalDevice& mPhysicalDevice;

        explicit QueueFamily(const blk::PhysicalDevice& vkphysicaldevice, std::uint32_t idx, VkQueueFamilyProperties properties)
            : mIndex(idx)
            , mProperties(properties)
            , mPhysicalDevice(vkphysicaldevice)
        {
        }

        bool supports_presentation() const;

        bool supports_surface(VkSurfaceKHR) const;

        operator VkQueueFamilyProperties() const
        {
            return mProperties;
        }
    };

    struct Queue
    {
        VkQueue                 mQueue;
        std::uint32_t           mIndex;
        const blk::QueueFamily& mFamily;

        explicit Queue(const blk::QueueFamily& queue_family, VkQueue vkqueue, std::uint32_t idx)
            : mQueue(vkqueue)
            , mIndex(idx)
            , mFamily(queue_family)
        {
        }

        operator VkQueue() const
        {
            return mQueue;
        }
    };
}
