#pragma once

#include <vulkan/vulkan_core.h>

namespace blk
{
struct Material
{
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
};
} // namespace blk
