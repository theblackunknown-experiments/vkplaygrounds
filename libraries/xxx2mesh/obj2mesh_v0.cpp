
#include "obj2mesh.hpp"

#include <cassert>
#include <cstddef>

#include <vkcpumesh.hpp>

#include <tiny_obj_loader.h>
#include <iostream>

namespace blk::meshes::obj
{
inline namespace v0
{

CPUMesh load(const fs::path& path)
{
	assert(fs::exists(path));
	assert(path.extension() == ".obj");

	tinyobj::ObjReader reader;
	tinyobj::ObjReaderConfig config;
	bool status = reader.ParseFromFile(path.generic_string(), config);
	assert(status);
	assert(reader.Valid());
	if (!reader.Warning().empty())
	{
		std::cerr << "WARN: " << reader.Warning() << std::endl;
	}
	if (!reader.Error().empty())
	{
		std::cerr << "ERROR: " << reader.Error() << std::endl;
		throw std::runtime_error(reader.Error());
	}

	CPUMesh cpumesh;
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

					cpumesh.mVertices.push_back(Vertex{
						.position = {vx, vy, vz},
						.normal = {nx, ny, nz},
						.color = {nx, ny, nz},
					});
				}
				else
				{
					cpumesh.mVertices.push_back(Vertex{
						.position = {vx, vy, vz},
						.color = {vx, vy, vz},
					});
				}
			}
			index_offset += fv;
		}
	}
	return cpumesh;
}
} // namespace v0
} // namespace blk::meshes::obj
