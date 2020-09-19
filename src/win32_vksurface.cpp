#include "./vksurface.hpp"

#include "./vkdebug.hpp"

namespace blk
{
    Surface Surface::create(VkInstance instance, const VkWin32SurfaceCreateInfoKHR& info)
    {
        VkSurfaceKHR vksurface;
        CHECK(vkCreateWin32SurfaceKHR(instance, &info, nullptr, &vksurface));
        return Surface(instance, vksurface, info);
    }
}
