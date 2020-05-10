#include <vulkan/vulkan.h>

#include <cassert>

#include <algorithm>

#include <sstream>
#include <iostream>

#include "vulkandebug.hpp"
#include "vulkanapplication.hpp"

VulkanApplication::VulkanApplication()
    : VulkanApplicationBase()
    , VulkanInstanceMixin()
{
    { // Instance
        std::vector<const char*> extensions(mExtensions.size());
        std::transform(
            mExtensions.begin(), mExtensions.end(),
            extensions.begin(),
            [](const VkExtensionProperties& e) {
                return e.extensionName;
            }
        );
        auto it = std::find_if(
            std::begin(extensions), std::end(extensions),
            [](const char* v) {
                return std::strncmp(v, VK_EXT_DEBUG_UTILS_EXTENSION_NAME, std::strlen(v)) == 0;
            }
        );
        assert(it != std::end(extensions));
        const char* const layers[] = {
            "VK_LAYER_KHRONOS_validation"
        };
        const VkApplicationInfo info_application{
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = nullptr,
            .pApplicationName   = "theblackunknown - Playground",
            .applicationVersion = VK_MAKE_VERSION(0,0,0),
            .pEngineName        = "theblackunknown",
            .engineVersion      = VK_MAKE_VERSION(0,0,0),
            .apiVersion         = VK_MAKE_VERSION(mVersion.major,mVersion.minor,mVersion.patch),
        };
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
            .enabledLayerCount       = 1,
            .ppEnabledLayerNames     = &layers[0],
            .enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
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
        // {
        //     const VkDebugUtilsMessengerCreateInfoEXT info{
        //         .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        //         .pNext           = nullptr,
        //         .flags           = 0,
        //         .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
        //         .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        //             | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        //             | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        //         .pfnUserCallback = StandardErrorDebugCallback,
        //         .pUserData       = nullptr,
        //     };
        //     CHECK(vkCreateDebugUtilsMessengerEXT(mInstance, &info, nullptr, &mStandardErrorMessenger));
        // }
    }
}

VulkanApplication::~VulkanApplication()
{
    {
        auto vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT"));
        // vkDestroyDebugUtilsMessengerEXT(mInstance, mStandardErrorMessenger, nullptr);
        vkDestroyDebugUtilsMessengerEXT(mInstance, mDebuggerMessenger, nullptr);
    }
    vkDestroyInstance(mInstance, nullptr);
}
