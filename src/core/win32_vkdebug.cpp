#include <windows.h>

#include <vulkan/vulkan.h>

#include <cassert>

#include <sstream>
#include <iostream>

#include "./vkdebug.hpp"

void CHECK(const VkResult& v)
{
    if ((v != VK_SUCCESS) && IsDebuggerPresent())
    {
        DebugBreak();
    }
    assert(v == VK_SUCCESS);
}

void CHECK(bool v)
{
    if (!v && IsDebuggerPresent())
    {
        DebugBreak();
    }
    assert(v);
}

VkBool32 StandardErrorDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    return OutputStreamDebugCallback(
        messageSeverity,
        messageType,
        pCallbackData,
        pUserData,
        std::cerr
    );
}

VkBool32 DebuggerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    std::ostringstream stream;
    auto status = OutputStreamDebugCallback(
        messageSeverity,
        messageType,
        pCallbackData,
        pUserData,
        stream
    );
    OutputDebugString(stream.str().c_str());

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        DebugBreak();
    }
    return status;
}
