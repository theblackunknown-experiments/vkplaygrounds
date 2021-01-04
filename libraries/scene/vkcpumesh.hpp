#pragma once

#include <glm/vec3.hpp>

#include <vector>

#include <scene_export.h>

namespace blk
{
struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
};

struct CPUMesh
{
	std::vector<Vertex> mVertices;
};
} // namespace blk
