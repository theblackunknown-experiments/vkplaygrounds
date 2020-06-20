#include <vulkan/vulkan.h>

#include <span>
#include <iterator>
#include <iostream>

#include "./vulkandebug.hpp"
#include "./vulkanutilities.hpp"

#include "./vulkangenerativeshader.hpp"

std::tuple<VkPhysicalDevice, std::uint32_t, std::uint32_t> VulkanGenerativeShader::requirements(const std::span<VkPhysicalDevice>& vkphysicaldevices)
{
    constexpr const std::uint32_t queue_count = 1;
    for (const VkPhysicalDevice& vkphysicaldevice : vkphysicaldevices) {
        std::uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
        assert(count > 0);

        std::vector<VkQueueFamilyProperties> queue_families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, queue_families.data());

        auto optional_queue_family_index = select_queue_family_index(queue_families, VK_QUEUE_COMPUTE_BIT);

        if (optional_queue_family_index.has_value())
            return std::make_tuple(vkphysicaldevice, optional_queue_family_index.value(), queue_count);
    }
    return std::make_tuple(VkPhysicalDevice{VK_NULL_HANDLE}, 0u, 0u);
}

VulkanGenerativeShader::VulkanGenerativeShader(
        VkPhysicalDevice vkphysicaldevice,
        std::uint32_t queue_family_index,
        std::uint32_t queue_count
    )
    : mPhysicalDevice(vkphysicaldevice)
    , mQueueFamilyIndex(queue_family_index)
{
    vkGetPhysicalDeviceFeatures(vkphysicaldevice, &mFeatures);
    vkGetPhysicalDeviceProperties(vkphysicaldevice, &mProperties);
    vkGetPhysicalDeviceMemoryProperties(vkphysicaldevice, &mMemoryProperties);
    {// Queue Family Properties
        std::uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
        assert(count > 0);
        mQueueFamiliesProperties.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, mQueueFamiliesProperties.data());
    }
    {// Extensions
        std::uint32_t count;
        CHECK(vkEnumerateDeviceExtensionProperties(vkphysicaldevice, nullptr, &count, nullptr));
        mExtensions.resize(count);
        CHECK(vkEnumerateDeviceExtensionProperties(vkphysicaldevice, nullptr, &count, mExtensions.data()));
    }

    std::cout << "VulkanGenerativeShader - GPU: " << DeviceType2Text(mProperties.deviceType) << std::endl;
    {// Version
        std::uint32_t major = VK_VERSION_MAJOR(mProperties.apiVersion);
        std::uint32_t minor = VK_VERSION_MINOR(mProperties.apiVersion);
        std::uint32_t patch = VK_VERSION_PATCH(mProperties.apiVersion);
        std::cout << mProperties.deviceName << ": " << major << "." << minor << "." << patch << std::endl;
        major = VK_VERSION_MAJOR(mProperties.driverVersion);
        minor = VK_VERSION_MINOR(mProperties.driverVersion);
        patch = VK_VERSION_PATCH(mProperties.driverVersion);
        std::cout << "Driver: " << major << "." << minor << "." << patch << std::endl;
    }

    {// Device
        const std::vector<float> priorities(queue_count, 0.0f);
        const VkDeviceQueueCreateInfo info_queue{
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .queueFamilyIndex = mQueueFamilyIndex,
            .queueCount       = queue_count,
            .pQueuePriorities = priorities.data(),
        };
        constexpr const VkPhysicalDeviceFeatures features{
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

        std::vector<const char*> enabled_extensions{};
        if (has_extension(mExtensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
        {// Debug Marker
            enabled_extensions.emplace_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        }
        const VkDeviceCreateInfo info{
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .queueCreateInfoCount    = 1,
            .pQueueCreateInfos       = &info_queue,
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            .enabledExtensionCount   = static_cast<std::uint32_t>(enabled_extensions.size()),
            .ppEnabledExtensionNames = enabled_extensions.data(),
            .pEnabledFeatures        = &features,
        };
        CHECK(vkCreateDevice(mPhysicalDevice, &info, nullptr, &mDevice));
    }
    vkGetDeviceQueue(mDevice, mQueueFamilyIndex, 0, &mQueue);
}

VulkanGenerativeShader::~VulkanGenerativeShader()
{
    vkDestroyDevice(mDevice, nullptr);
}
