#pragma once

#include <vulkan/vulkan_core.h>

#include <scene_export.h>

namespace blk
{
struct Memory;
struct Device;

struct CPUMesh;
struct GPUMesh;

SCENE_EXPORT GPUMesh upload_host_visible(const blk::Device& vkdevice, const CPUMesh& mesh, blk::Memory& memory);

} // namespace blk
