#pragma once

#include <vulkan/vulkan.h>

namespace blk
{
    struct Surface
    {
        #ifdef OS_WINDOWS
        using info_t = VkWin32SurfaceCreateInfoKHR;
        #endif

        explicit Surface(VkInstance instance, VkSurfaceKHR surface, const info_t& info)
            : mInstance(instance)
            , mSurface(surface)
            , mInfo(info)
        {
        }
        ~Surface()
        {
            vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        }

        operator VkSurfaceKHR() const
        {
            return mSurface;
        }

        VkInstance   mInstance = VK_NULL_HANDLE;
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;
        info_t       mInfo;

        static
        Surface create(VkInstance instance, const info_t& info);
    };
}
