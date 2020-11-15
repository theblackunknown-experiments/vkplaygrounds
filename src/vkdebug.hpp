#pragma once

#include <string>
#include <ostream>

#include <vulkan/vulkan.h>

void CHECK(const VkResult& v);
void CHECK(bool v);

VKAPI_ATTR VkBool32 VKAPI_CALL DebuggerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

VKAPI_ATTR VkBool32 VKAPI_CALL StandardErrorDebugCallback(
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
constexpr const char * VkDebugUtilsMessageSeverityFlagBitsEXTString(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity);constexpr const char * VkDebugUtilsMessageSeverityFlagBitsEXTString(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity)
{
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        return "VERBOSE";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        return "INFO";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        return "WARNING";
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        return "ERROR";
    }
    else
    {
        return "UNKNOWN";
    }
}

const std::string VkDebugUtilsMessageTypeFlagsEXTString(const VkDebugUtilsMessageTypeFlagsEXT messageType);
