#include <vulkan/vulkan.h>

#include <cassert>

#include <algorithm>

#include <array>

#include <sstream>
#include <iostream>

#include "./vkdebug.hpp"
#include "./vkapplication.hpp"

namespace
{
    constexpr const std::array kExtensions{
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        "VK_KHR_surface",
        "VK_KHR_win32_surface",
    };
    constexpr const std::array kLayers{
        "VK_LAYER_KHRONOS_validation",
    };
}

VulkanApplication::VulkanApplication(Version version)
    : mVersion(version)
{
    { // Layers / Extensions
        std::uint32_t count;
        CHECK(vkEnumerateInstanceLayerProperties(&count, nullptr));
        mLayers.resize(count);
        CHECK(vkEnumerateInstanceLayerProperties(&count, mLayers.data()));
    }
    {
        std::uint32_t count;
        CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
        mExtensions.resize(count);
        CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, mExtensions.data()));

        for (auto&& layer_properties : mLayers)
        {
            CHECK(vkEnumerateInstanceExtensionProperties(layer_properties.layerName, &count, nullptr));
            auto& layer_extensions = mLayerExtensions[layer_properties.layerName];
            layer_extensions.resize(count);
            CHECK(vkEnumerateInstanceExtensionProperties(layer_properties.layerName, &count, layer_extensions.data()));
        }
    }
    { // Instance
        const VkApplicationInfo info_application{
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = nullptr,
            .pApplicationName   = "theblackunknown - vkplaygrounds",
            .applicationVersion = VK_MAKE_VERSION(0,0,0),
            .pEngineName        = "theblackunknown",
            .engineVersion      = VK_MAKE_VERSION(0,0,0),
            .apiVersion         = mVersion,
        };

        // NOTE Un-comment if required
        // constexpr const VkValidationCheckEXT kDisabledChecks[] = {
        //     VK_VALIDATION_CHECK_ALL_EXT,
        //     VK_VALIDATION_CHECK_SHADERS_EXT,
        // };
        // const VkValidationFlagsEXT info_validation_check{
        //     .sType                        = VK_STRUCTURE_TYPE_VALIDATION_FLAGS_EXT,
        //     .pNext                        = nullptr,
        //     .disabledValidationCheckCount = sizeof(kDisabledChecks) / sizeof(*kDisabledChecks),
        //     .pDisabledValidationChecks    = kDisabledChecks,
        // };

        // NOTE
        //  - https://www.lunarg.com/new-tutorial-for-vulkan-debug-utilities-extension/
        //  - https://www.lunarg.com/wp-content/uploads/2018/05/Vulkan-Debug-Utils_05_18_v1.pdf
        //  - https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_EXT_debug_utils.html
        //  - https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_EXT_debug_report.html
        //  - https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkDebugUtilsMessengerCreateInfoEXT.html
        const VkDebugUtilsMessengerCreateInfoEXT info_debug{
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext           = nullptr,
            .flags           = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = StandardErrorDebugCallback,
            .pUserData       = nullptr,
        };
        const VkInstanceCreateInfo info_instance{
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                   = &info_debug,
            .flags                   = 0,
            .pApplicationInfo        = &info_application,
            .enabledLayerCount       = kLayers.size(),
            .ppEnabledLayerNames     = kLayers.data(),
            .enabledExtensionCount   = kExtensions.size(),
            .ppEnabledExtensionNames = kExtensions.data(),
        };
        CHECK(vkCreateInstance(&info_instance, nullptr, &mInstance));
    }
    {
        auto vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT"));
        {
            const VkDebugUtilsMessengerCreateInfoEXT info{
                .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .pNext           = nullptr,
                .flags           = 0,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = StandardErrorDebugCallback,
                .pUserData       = nullptr,
            };
            CHECK(vkCreateDebugUtilsMessengerEXT(mInstance, &info, nullptr, &mStandardErrorMessenger));
        }
        {
            const VkDebugUtilsMessengerCreateInfoEXT info{
                .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .pNext           = nullptr,
                .flags           = 0,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = DebuggerCallback,
                .pUserData       = nullptr,
            };
            CHECK(vkCreateDebugUtilsMessengerEXT(mInstance, &info, nullptr, &mDebuggerMessenger));
        }
    }
}

VulkanApplication::~VulkanApplication()
{
    {
        auto vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT"));
        vkDestroyDebugUtilsMessengerEXT(mInstance, mDebuggerMessenger, nullptr);
        vkDestroyDebugUtilsMessengerEXT(mInstance, mStandardErrorMessenger, nullptr);
    }
    vkDestroyInstance(mInstance, nullptr);
}
