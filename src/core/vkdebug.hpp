#pragma once

#include <string>
#include <ostream>

#include <vulkan/vulkan.h>

#include <core_export.h>

CORE_EXPORT void CHECK(const VkResult& v);
CORE_EXPORT void CHECK(bool v);

CORE_EXPORT VKAPI_ATTR VkBool32 VKAPI_CALL DebuggerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

CORE_EXPORT VKAPI_ATTR VkBool32 VKAPI_CALL StandardErrorDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

VkBool32 OutputStreamDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData,
    std::ostream& stream);

inline
constexpr const char * VkDebugUtilsMessageSeverityFlagBitsEXTString(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity)
{
    switch(messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return "VERBOSE";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT   : return "INFO";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return "WARNING";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT  : return "ERROR";
        default: return "UNKNOWN";
    }
}

inline
constexpr const char * VkDebugUtilsMessageTypeFlagsEXTString(const VkDebugUtilsMessageTypeFlagsEXT messageType)
{
    switch(messageType)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:                                                      return "GENERAL";
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: return "VALIDATION/PERFORMANCE";
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:                                                   return "VALIDATION";
        default: return "";
    }
}
