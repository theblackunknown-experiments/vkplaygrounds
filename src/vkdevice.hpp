#pragma once

#include <vulkan/vulkan_core.h>

#include "./vkdebug.hpp"

namespace blk
{
    struct Device
    {
        VkDeviceCreateInfo mInfo;
        VkDevice           mDevice = VK_NULL_HANDLE;

        constexpr Device() = default;

        constexpr Device(const Device& rhs) = delete;

        constexpr Device(Device&& rhs)
            : mInfo(std::move(rhs.mInfo))
            , mDevice(std::exchange(rhs.mDevice, VkDevice{VK_NULL_HANDLE}))
        {
        }

        constexpr explicit Device(const VkDeviceCreateInfo& info)
            : mInfo(info)
        {
        }

        ~Device()
        {
            destroy();
        }

        Device& operator=(const Device& rhs) = delete;

        Device& operator=(Device&& rhs)
        {
            mInfo         = std::exchange(rhs.mInfo        , mInfo);
            mDevice       = std::exchange(rhs.mDevice      , mDevice);
            return *this;
        }

        VkResult create(VkPhysicalDevice vkphysicaldevice)
        {
            auto result = vkCreateDevice(vkphysicaldevice, &mInfo, nullptr, &mDevice);
            CHECK(result);
            return result;
        }

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
