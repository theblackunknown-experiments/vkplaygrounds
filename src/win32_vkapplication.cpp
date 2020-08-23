#include "./win32_vkapplication.hpp"

#include "./vkdebug.hpp"
#include "./vkutilities.hpp"

#include <vulkan/vulkan_core.h>

#include <cassert>

VulkanWin32Application::VulkanWin32Application(HINSTANCE hInstance, HWND hWindow)
    : VulkanApplication()
    , hInstance(hInstance)
    , hWindow(hWindow)
{
    assert(has_extension(mExtensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME));
    const VkWin32SurfaceCreateInfoKHR info{
        .sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext     = nullptr,
        .flags     = 0,
        .hinstance = hInstance,
        .hwnd      = hWindow,
    };
    CHECK(vkCreateWin32SurfaceKHR(mInstance, &info, nullptr, &mSurface));
}

VulkanWin32Application::~VulkanWin32Application()
{
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
}
