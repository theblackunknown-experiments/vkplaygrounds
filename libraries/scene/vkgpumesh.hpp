#pragma once

#include <cstddef>

#include <vkbuffer.hpp>

#include <scene_export.h>

namespace blk
{
struct GPUMesh
{
	std::size_t mVertexCount;
	blk::Buffer mVertexBuffer;
};
} // namespace blk
