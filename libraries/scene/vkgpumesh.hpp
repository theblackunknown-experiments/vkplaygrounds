#pragma once

#include <cstddef>

#include <vkbuffer.hpp>

namespace blk
{
struct GPUMesh
{
    std::size_t mVertexCount;
    blk::Buffer mVertexBuffer;
};
} // namespace blk
