#include <windows.h>

#include <vulkan/vulkan.h>

#include <cassert>

#include <sstream>
#include <iostream>

#include "./vkdebug.hpp"

const std::string VkDebugUtilsMessageTypeFlagsEXTString(const VkDebugUtilsMessageTypeFlagsEXT messageType) {
    std::ostringstream stream;
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        stream << "GENERAL";
    }
    else
    {
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            stream << "VALIDATION";
        }
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
            if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
                stream << '|';
            }
            stream << "PERFORMANCE";
        }
    }
    return stream.str();
}

VkBool32 OutputStreamDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData,
    std::ostream& stream) {
    stream
        << VkDebugUtilsMessageSeverityFlagBitsEXTString(messageSeverity) << " : "
        << VkDebugUtilsMessageTypeFlagsEXTString(messageType) << std::endl;
    stream
        << "Message ID Number " << pCallbackData->messageIdNumber << std::endl;
    if (pCallbackData->pMessageIdName)
        stream
            << "Message ID String " << pCallbackData->pMessageIdName  << std::endl;
    stream
        << pCallbackData->pMessage << std::endl;

    if (pCallbackData->objectCount > 0) {
        stream
            << '\n'
            << "Objects - " << pCallbackData->objectCount
            << '\n';

        for (std::uint32_t object = 0; object < pCallbackData->objectCount; ++object) {
            const VkDebugUtilsObjectNameInfoEXT& pObject = pCallbackData->pObjects[object];
            stream
                << "\tObject[" << object << "] - Type " << pObject.objectType << ", Value " << pObject.objectHandle;
            if (pObject.pObjectName)
            {
                stream << ", Name \"" << pObject.pObjectName << '\"';
            }
            stream << '\n';
        }
    }
    if (pCallbackData->cmdBufLabelCount > 0) {
        stream
            << '\n'
            << "Command Buffer Labels - " << pCallbackData->cmdBufLabelCount
            << '\n';

        for (std::uint32_t label = 0; label < pCallbackData->cmdBufLabelCount; ++label) {
            const VkDebugUtilsLabelEXT& pLabel = pCallbackData->pCmdBufLabels[label];
            stream
                << "\tLabel[" << label << "] - "
                << pLabel.pLabelName << ' '
                << "{ " << pLabel.color[0] << ", " << pLabel.color[1] << ", " << pLabel.color[2] << ", " << pLabel.color[3] << "}";
        }
    }

    stream << std::endl;

    // True is reserved for layer developers, and MAY mean calls are not distributed down the layer chain after validation error.
    // False SHOULD always be returned by apps:
    return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL StandardErrorDebugCallback(
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
