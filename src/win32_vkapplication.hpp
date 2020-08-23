#pragma once

#include "./vkapplication.hpp"

#include <windows.h>

struct VulkanWin32Application : public VulkanApplication
{
    explicit VulkanWin32Application(HINSTANCE hInstance, HWND hWindow);
    ~VulkanWin32Application();

    HINSTANCE    hInstance;
    HWND         hWindow;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
};
