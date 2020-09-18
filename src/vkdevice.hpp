#pragma once

#include <vulkan/vulkan_core.h>

#include "./vkdebug.hpp"

namespace blk
{
    struct PhysicalDevice;

    struct Device
    {
        VkDeviceCreateInfo    mInfo;
        VkDevice              mDevice = VK_NULL_HANDLE;
        const PhysicalDevice* mPhysicalDevice = nullptr;

        constexpr Device() = default;

        constexpr Device(const Device& rhs) = delete;

        constexpr Device(Device&& rhs)
            : mInfo(std::move(rhs.mInfo))
            , mDevice(std::exchange(rhs.mDevice, VkDevice{VK_NULL_HANDLE}))
            , mPhysicalDevice(std::exchange(rhs.mPhysicalDevice, nullptr))
        {
        }

        constexpr explicit Device(const blk::PhysicalDevice& physicaldevice, const VkDeviceCreateInfo& info)
            : mInfo(info)
            , mPhysicalDevice(&physicaldevice)
        {
        }

        ~Device()
        {
            destroy();
        }

        Device& operator=(const Device& rhs) = delete;

        Device& operator=(Device&& rhs)
        {
            mInfo           = std::exchange(rhs.mInfo        , mInfo);
            mDevice         = std::exchange(rhs.mDevice      , mDevice);
            mPhysicalDevice = std::exchange(rhs.mPhysicalDevice      , mPhysicalDevice);
            return *this;
        }

        VkResult create();

        void destroy()
        {
            if (mDevice != VK_NULL_HANDLE)
                vkDestroyDevice(mDevice, nullptr);
        }

        constexpr bool created() const
        {
            return mDevice != VK_NULL_HANDLE;
        }

        constexpr operator VkDevice() const
        {
            return mDevice;
        }
    };
}
