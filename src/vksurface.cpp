#include "./vksurface.hpp"

#include <vulkan/vulkan_core.h>

VulkanSurface::VulkanSurface(VkInstance instance, VkSurfaceKHR surface)
    : mInstance(instance)
    , mSurface(surface)
{
}

VulkanSurface::~VulkanSurface()
{
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
}
