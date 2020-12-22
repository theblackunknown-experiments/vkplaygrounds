
#include "obj2mesh.hpp"

#include <cassert>
#include <cstddef>

#include <optional>

namespace
{
    constexpr std::size_t kIndexVertex = 0;
    constexpr std::size_t kIndexPoint = 1;
}

namespace blk::meshes::obj
{

result_t wrap_as_buffers(const obj_t& obj)
{
    result_t result;

    std::optional<std::size_t> index_texcoord;
    std::optional<std::size_t> index_normal;

    std::size_t shared_attribute_count = 2; // Vertex + Point
    if (!obj.texcoords.empty())
    {
        index_texcoord = shared_attribute_count;
        shared_attribute_count += 1;
    }
    if (!obj.normals.empty())
    {
        index_normal = shared_attribute_count;
        shared_attribute_count += 1;
    }

    const std::size_t buffer_count = shared_attribute_count + obj.groups.size();

    static_assert(std::is_same_v<decltype(obj.vertices)::value_type, blk::meshes::obj::vertex_t>);
    result.vertices.pointer         = obj.vertices.data();
    result.vertices.count           = obj.vertices.size();
    result.vertices.stride          = sizeof(blk::meshes::obj::vertex_t);
    result.vertices.element_stride  = sizeof(float);
    result.vertices.usage           = blk::AttributeUsage::Vertex;
    result.vertices.datatype        = blk::AttributeDataType::Float;

    static_assert(std::is_same_v<decltype(obj.points)::value_type, blk::meshes::obj::point_t>);
    result.points.pointer        = obj.points.data();
    result.points.count          = obj.points.size();
    result.points.stride         = sizeof(blk::meshes::obj::point_t);
    result.points.element_stride = sizeof(std::size_t);
    result.points.usage          = blk::AttributeUsage::Point;
    result.points.datatype       = blk::AttributeDataType::Unsigned;

    if (index_texcoord)
    {
        static_assert(std::is_same_v<decltype(obj.texcoords)::value_type, blk::meshes::obj::texcoord_t>);
        result.texcoords = blk::BufferCPU{
            .pointer        = obj.texcoords.data(),
            .count          = obj.texcoords.size(),
            .stride         = sizeof(blk::meshes::obj::texcoord_t),
            .element_stride = sizeof(float),
            .usage          = blk::AttributeUsage::TexCoord,
            .datatype       = blk::AttributeDataType::Float,
        };
    }

    if (index_normal)
    {
        static_assert(std::is_same_v<decltype(obj.normals)::value_type, blk::meshes::obj::normal_t>);
        result.normals = blk::BufferCPU{
            .pointer        = obj.normals.data(),
            .count          = obj.normals.size(),
            .stride         = sizeof(blk::meshes::obj::normal_t),
            .element_stride = sizeof(float),
            .usage          = blk::AttributeUsage::Normal,
            .datatype       = blk::AttributeDataType::Float,
        };
    }

    result.faces_by_group.reserve(obj.groups.size());
    for (const group_t& group : obj.groups)
    {
        if (group.faces.empty())
            continue;

        static_assert(std::is_same_v<decltype(group.faces)::value_type, blk::meshes::obj::face_t>);
        result.faces_by_group.emplace(group.name, blk::BufferCPU{
            .pointer        = group.faces.data(),
            .count          = group.faces.size(),
            .stride         = sizeof(blk::meshes::obj::face_t),
            .element_stride = sizeof(std::size_t),
            .usage          = blk::AttributeUsage::Face,
            .datatype       = blk::AttributeDataType::Unsigned,
        });
    }

    return result;
}

}
