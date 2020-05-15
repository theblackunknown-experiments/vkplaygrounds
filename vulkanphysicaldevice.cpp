#include <vulkan/vulkan.h>

#include <cassert>

#include <fstream>
#include <iostream>

#include <iterator>
#include <algorithm>

#include "vulkandebug.hpp"

#include "vulkanphysicaldevice.hpp"

VulkanPhysicalDevice::VulkanPhysicalDevice(const VkPhysicalDevice& physical_device)
    : VulkanPhysicalDeviceBase(physical_device)
    , VulkanPhysicalDeviceMixin()
    , mPhysicalDevice(physical_device)
{
    { // Version
        mVersion.major = VK_VERSION_MAJOR(mProperties.apiVersion);
        mVersion.minor = VK_VERSION_MINOR(mProperties.apiVersion);
        mVersion.patch = VK_VERSION_PATCH(mProperties.apiVersion);
        std::cout << mProperties.deviceName << ": " << mVersion.major << "." << mVersion.minor << "." << mVersion.patch << std::endl;
        mDriverVersion.major = VK_VERSION_MAJOR(mProperties.driverVersion);
        mDriverVersion.minor = VK_VERSION_MINOR(mProperties.driverVersion);
        mDriverVersion.patch = VK_VERSION_PATCH(mProperties.driverVersion);
        std::cout << "Driver: " << mDriverVersion.major << "." << mDriverVersion.minor << "." << mDriverVersion.patch << std::endl;
    }
}

std::tuple<std::uint32_t, VkDevice> VulkanPhysicalDevice::create_device(VkQueueFlags queue_requirements, bool swap_chain)
{
    auto optional_family_index = select_queue_family_index(queue_requirements);
    assert(optional_family_index.has_value());

    auto queue_family_index = optional_family_index.value();

    const float priorities[] = {0.0f};
    VkDeviceQueueCreateInfo info_queue{
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext            = nullptr,
        .flags            = 0,
        .queueFamilyIndex = queue_family_index,
        .queueCount       = 1,
        .pQueuePriorities = priorities,
    };
    VkPhysicalDeviceFeatures features{
        .robustBufferAccess                      = VK_FALSE,
        .fullDrawIndexUint32                     = VK_FALSE,
        .imageCubeArray                          = VK_FALSE,
        .independentBlend                        = VK_FALSE,
        .geometryShader                          = VK_FALSE,
        .tessellationShader                      = VK_FALSE,
        .sampleRateShading                       = VK_FALSE,
        .dualSrcBlend                            = VK_FALSE,
        .logicOp                                 = VK_FALSE,
        .multiDrawIndirect                       = VK_FALSE,
        .drawIndirectFirstInstance               = VK_FALSE,
        .depthClamp                              = VK_FALSE,
        .depthBiasClamp                          = VK_FALSE,
        .fillModeNonSolid                        = VK_FALSE,
        .depthBounds                             = VK_FALSE,
        .wideLines                               = VK_FALSE,
        .largePoints                             = VK_FALSE,
        .alphaToOne                              = VK_FALSE,
        .multiViewport                           = VK_FALSE,
        .samplerAnisotropy                       = VK_FALSE,
        .textureCompressionETC2                  = VK_FALSE,
        .textureCompressionASTC_LDR              = VK_FALSE,
        .textureCompressionBC                    = VK_FALSE,
        .occlusionQueryPrecise                   = VK_FALSE,
        .pipelineStatisticsQuery                 = VK_FALSE,
        .vertexPipelineStoresAndAtomics          = VK_FALSE,
        .fragmentStoresAndAtomics                = VK_FALSE,
        .shaderTessellationAndGeometryPointSize  = VK_FALSE,
        .shaderImageGatherExtended               = VK_FALSE,
        .shaderStorageImageExtendedFormats       = VK_FALSE,
        .shaderStorageImageMultisample           = VK_FALSE,
        .shaderStorageImageReadWithoutFormat     = VK_FALSE,
        .shaderStorageImageWriteWithoutFormat    = VK_FALSE,
        .shaderUniformBufferArrayDynamicIndexing = VK_FALSE,
        .shaderSampledImageArrayDynamicIndexing  = VK_FALSE,
        .shaderStorageBufferArrayDynamicIndexing = VK_FALSE,
        .shaderStorageImageArrayDynamicIndexing  = VK_FALSE,
        .shaderClipDistance                      = VK_FALSE,
        .shaderCullDistance                      = VK_FALSE,
        .shaderFloat64                           = VK_FALSE,
        .shaderInt64                             = VK_FALSE,
        .shaderInt16                             = VK_FALSE,
        .shaderResourceResidency                 = VK_FALSE,
        .shaderResourceMinLod                    = VK_FALSE,
        .sparseBinding                           = VK_FALSE,
        .sparseResidencyBuffer                   = VK_FALSE,
        .sparseResidencyImage2D                  = VK_FALSE,
        .sparseResidencyImage3D                  = VK_FALSE,
        .sparseResidency2Samples                 = VK_FALSE,
        .sparseResidency4Samples                 = VK_FALSE,
        .sparseResidency8Samples                 = VK_FALSE,
        .sparseResidency16Samples                = VK_FALSE,
        .sparseResidencyAliased                  = VK_FALSE,
        .variableMultisampleRate                 = VK_FALSE,
        .inheritedQueries                        = VK_FALSE,
    };
    std::vector<const char*> enable_extensions{};
    // TODO Make this optional
    if(swap_chain)
    {
        assert(has_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME));
        enable_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    if(has_extension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
    {
        enable_extensions.emplace_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    }
    VkDeviceCreateInfo info_device{
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &info_queue,
        .enabledLayerCount       = 0,
        .ppEnabledLayerNames     = nullptr,
        .enabledExtensionCount   = static_cast<std::uint32_t>(enable_extensions.size()),
        .ppEnabledExtensionNames = enable_extensions.data(),
        .pEnabledFeatures        = &features,
    };
    VkDevice device;
    CHECK(vkCreateDevice(mPhysicalDevice, &info_device, nullptr, &device));
    return std::make_tuple(queue_family_index, device);
}

VulkanPhysicalDevice::~VulkanPhysicalDevice() = default;
