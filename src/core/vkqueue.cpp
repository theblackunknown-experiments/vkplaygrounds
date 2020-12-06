#include "./vkqueue.hpp"

#include "./vkdebug.hpp"
#include "./vkphysicaldevice.hpp"

#include <vulkan/vulkan_core.h>

namespace blk
{

bool QueueFamily::supports_presentation() const
{
    #ifdef OS_WINDOWS
    if (!vkGetPhysicalDeviceWin32PresentationSupportKHR(mPhysicalDevice, mIndex))
        return false;
    #endif

    return mProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
}

bool QueueFamily::supports_surface(VkSurfaceKHR vksurface) const
{
    VkBool32 vksupport = VK_FALSE;
    CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, mIndex, vksurface, &vksupport));
    return vksupport;
}

}
