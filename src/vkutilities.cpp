#include "./vkutilities.hpp"

#include "./vkqueue.hpp"
#include "./vkphysicaldevice.hpp"

namespace blk
{

std::vector<blk::PhysicalDevice> physicaldevices(VkInstance instance)
{
    std::uint32_t count = 0;
    CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    assert(count > 0);

    std::vector<VkPhysicalDevice> vkphysicaldevices(count);
    CHECK(vkEnumeratePhysicalDevices(instance, &count, vkphysicaldevices.data()));

    std::vector<blk::PhysicalDevice> physicaldevices;
    physicaldevices.reserve(count);
    for (auto&& vkphysicaldevice : vkphysicaldevices)
        physicaldevices.emplace_back(vkphysicaldevice);
    return physicaldevices;
}

}
