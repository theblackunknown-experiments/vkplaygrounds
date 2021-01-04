#pragma once

#include <cstddef>

#include <vulkan/vulkan_core.h>

#include <scene_export.h>

namespace blk
{
struct GPUMesh
{
	std::size_t mVertexCount;
	VkBuffer mVertexBuffer;
};
} // namespace blk
