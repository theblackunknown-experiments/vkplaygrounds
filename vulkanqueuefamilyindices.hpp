#pragma once

#include <cstddef>

#include <vulkan/vulkan_core.h>

struct VulkanQueueFamilyIndices
{
    std::uint32_t engine;
    std::uint32_t dearimgui;
    std::uint32_t presentation;
    std::uint32_t generativeshader;
};
