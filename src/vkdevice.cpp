#include "./vkdevice.hpp"
#include "./vkphysicaldevice.hpp"

#include <vulkan/vulkan_core.h>

namespace blk
{

VkResult Device::create()
{
    auto result = vkCreateDevice(*mPhysicalDevice, &mInfo, nullptr, &mDevice);
    CHECK(result);
    return result;
}

}
