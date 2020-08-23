#pragma once

#include <vulkan/vulkan_core.h>

#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include "./vkdebug.hpp"

inline
[[nodiscard]]
VkSurfaceKHR create_surface(VkInstance instance, HINSTANCE hInstance, HWND hWindow)
{
    const VkWin32SurfaceCreateInfoKHR info{
        .sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext     = nullptr,
        .flags     = 0,
        .hinstance = hInstance,
        .hwnd      = hWindow,
    };
    VkSurfaceKHR vksurface;
    CHECK(vkCreateWin32SurfaceKHR(instance, &info, nullptr, &vksurface));
    return vksurface;
}
