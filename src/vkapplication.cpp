#include <vulkan/vulkan.h>

#include <cassert>

#include <algorithm>

#include <sstream>
#include <iostream>

#include "./vkdebug.hpp"
#include "./vkapplication.hpp"

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

        constexpr const VkValidationFeatureEnableEXT kFeaturesEnabled[] = {
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
            VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
            // NOTE Cannot be used with VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT
            // VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
        };
        // constexpr const VkValidationFeatureDisableEXT kFeaturesDisabled[] = {
        //     VK_VALIDATION_FEATURE_DISABLE_ALL_EXT,
        //     VK_VALIDATION_FEATURE_DISABLE_SHADERS_EXT,
        //     VK_VALIDATION_FEATURE_DISABLE_THREAD_SAFETY_EXT,
        //     VK_VALIDATION_FEATURE_DISABLE_API_PARAMETERS_EXT,
        //     VK_VALIDATION_FEATURE_DISABLE_OBJECT_LIFETIMES_EXT,
        //     VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT,
        //     VK_VALIDATION_FEATURE_DISABLE_UNIQUE_HANDLES_EXT,
        // };
        const VkValidationFeaturesEXT info_validation_features{
            .sType                          = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
            .pNext                          = nullptr,
            .enabledValidationFeatureCount  = sizeof(kFeaturesEnabled) / sizeof(*kFeaturesEnabled),
            .pEnabledValidationFeatures     = kFeaturesEnabled,
            // .disabledValidationFeatureCount = sizeof(kFeaturesDisabled) / sizeof(*kFeaturesDisabled),
            // .pDisabledValidationFeatures    = kFeaturesDisabled,
            .disabledValidationFeatureCount = 0,
            .pDisabledValidationFeatures    = nullptr,
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
