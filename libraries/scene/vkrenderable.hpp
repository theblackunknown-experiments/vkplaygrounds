#pragma once

#include <glm/mat4x4.hpp>

#include <vulkan/vulkan_core.h>

namespace blk
{
struct GPUMesh;
struct Material;

struct Renderable
{
    const GPUMesh* mMesh;
    const Material* mMaterial;
    glm::mat4 transform;
};
} // namespace blk
