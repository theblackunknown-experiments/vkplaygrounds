#pragma once

#include <scene_export.h>

namespace blk
{
struct Device;
struct Buffer;

struct CPUMesh;

SCENE_EXPORT void upload_host_visible(const blk::Device& vkdevice, const CPUMesh& cpumesh, blk::Buffer& vertex_buffer);

} // namespace blk
