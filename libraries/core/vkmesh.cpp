#include "./vkmesh.hpp"

#include <vulkan/vulkan_core.h>

#include <tiny_obj_loader.h>
#include <iostream>

namespace blk
{
VertexInputDescription Vertex::get_description()
{
	return VertexInputDescription{
		.bindings = {VkVertexInputBindingDescription{
			.binding = 0, .stride = sizeof(blk::Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}},
		.attributes = {
			VkVertexInputAttributeDescription{
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = offsetof(blk::Vertex, position),
			},
			VkVertexInputAttributeDescription{
				.location = 1,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = offsetof(blk::Vertex, normal),
			},
			VkVertexInputAttributeDescription{
				.location = 2,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = offsetof(blk::Vertex, color),
			},
		}};
}

bool Mesh::load_obj(const char* filename)
{
	tinyobj::ObjReader reader;
	tinyobj::ObjReaderConfig config;
	bool status = reader.ParseFromFile(filename, config);
	assert(status);
	assert(reader.Valid());
	if (!reader.Warning().empty())
	{
		std::cerr << "WARN: " << reader.Warning() << std::endl;
	}
	if (!reader.Error().empty())
	{
		std::cerr << "ERROR: " << reader.Error() << std::endl;
		return false;
	}

	for (auto&& shape : reader.GetShapes())
	{
		std::size_t index_offset = 0;
		for (auto&& vertex_count_per_face : shape.mesh.num_face_vertices)
		{
			constexpr int fv = 3;
			for (std::size_t v = 0; v < fv; ++v)
			{
				tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

				tinyobj::real_t vx = reader.GetAttrib().vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = reader.GetAttrib().vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = reader.GetAttrib().vertices[3 * idx.vertex_index + 2];

				if (!reader.GetAttrib().normals.empty())
				{
					tinyobj::real_t nx = reader.GetAttrib().normals[3 * idx.normal_index + 0];
					tinyobj::real_t ny = reader.GetAttrib().normals[3 * idx.normal_index + 1];
					tinyobj::real_t nz = reader.GetAttrib().normals[3 * idx.normal_index + 2];

					mVertices.push_back(Vertex{
						.position = {vx, vy, vz},
						.normal = {nx, ny, nz},
						.color = {nx, ny, nz},
					});
				}
				else
				{
					mVertices.push_back(Vertex{
						.position = {vx, vy, vz},
						.color = {vx, vy, vz},
					});
				}
			}
			index_offset += fv;
		}
	}
	return true;
}
} // namespace blk
