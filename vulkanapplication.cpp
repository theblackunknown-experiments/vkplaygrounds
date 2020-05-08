#include <vulkan/vulkan.h>

#include <cassert>

#include <algorithm>

#include <iostream>

#include "vulkandebug.hpp"
#include "vulkanapplication.hpp"

namespace
{
    inline
    const char *VkDebugReportFlagsEXTString(const VkDebugReportFlagsEXT flags) {
        switch (flags) {
            case VK_DEBUG_REPORT_ERROR_BIT_EXT:
                return "ERROR";
            case VK_DEBUG_REPORT_WARNING_BIT_EXT:
                return "WARNING";
            case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
                return "PERF";
            case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
                return "INFO";
            case VK_DEBUG_REPORT_DEBUG_BIT_EXT:
                return "DEBUG";
            default:
                return "UNKNOWN";
        }
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugReportFlagsEXT msgFlags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t srcObject,
        size_t location,
        int32_t msgCode,
        const char *pLayerPrefix,
        const char *pMsg,
        void *pUserData) {

        std::cerr
            << VkDebugReportFlagsEXTString(msgFlags)
            << ": [" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg << std::endl;

        // True is reserved for layer developers, and MAY mean calls are not distributed down the layer chain after validation error.
        // False SHOULD always be returned by apps:
        return VK_FALSE;
    }
}

VulkanApplication::VulkanApplication()
    : VulkanApplicationBase()
    , VulkanInstanceMixin()
{
    { // Instance
        std::vector<const char*> instance_extensions(mExtensions.size());
        std::transform(
            mExtensions.begin(), mExtensions.end(),
            instance_extensions.begin(),
            [](const VkExtensionProperties& e) {
                return e.extensionName;
            }
        );
        const VkDebugReportCallbackCreateInfoEXT info_debug{
            .sType       = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
            .pNext       = nullptr,
            .flags       = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
            .pfnCallback = DebugCallback,
            .pUserData   = this,
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
        // TODO VK_EXT_debug_report -> VK_EXT_debug_utils
        //  - https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkDebugUtilsMessengerCreateInfoEXT.html
        const VkInstanceCreateInfo info_instance{
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                   = &info_debug,
            .flags                   = 0,
            .pApplicationInfo        = &info_application,
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            .enabledExtensionCount   = static_cast<std::uint32_t>(instance_extensions.size()),
            .ppEnabledExtensionNames = instance_extensions.data(),
        };
        CHECK(vkCreateInstance(&info_instance, nullptr, &mInstance));
    }
}

VulkanApplication::~VulkanApplication()
{
    vkDestroyInstance(mInstance, nullptr);
}
