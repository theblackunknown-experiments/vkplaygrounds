
#include <cstdlib>
#include <cassert>

#include <fstream>
#include <iostream>
#include <charconv>
#include <type_traits>

#include <span>
#include <memory>
#include <string>
#include <iterator>
#include <optional>

#include <filesystem>

#include <vkmesh.hpp>

#include <range/v3/view/zip.hpp>

namespace fs = std::filesystem;

namespace
{
    enum class face_attribute_t
    {
        Point,
        Vertex,
        TexCoord,
        Normal

    };

    struct vertex_t {
        float x, y, z;
    };

    struct texcoord_t {
        float u, v;
    };

    struct normal_t {
        float x, y, z;
    };

    struct point_t {
        std::size_t   vertex = static_cast<std::size_t>(~0);
        std::size_t texcoord = static_cast<std::size_t>(~0);
        std::size_t   normal = static_cast<std::size_t>(~0);
    };

    struct face_t {
        std::size_t points[4] = {
            static_cast<std::size_t>(~0),
            static_cast<std::size_t>(~0),
            static_cast<std::size_t>(~0),
            static_cast<std::size_t>(~0)
        };
        // TODO ngons
    };

    struct group_t {
        std::string         name;
        std::string         material;
        std::vector<face_t> faces;
    };

    struct obj_t
    {
        fs::path                material_library;
        std::vector<vertex_t>   vertices;
        std::vector<texcoord_t> texcoords;
        std::vector<normal_t>   normals;
        std::vector<point_t>    points;
        std::vector<group_t>    groups;
    };

    bool is_face_attribute_separator(char c)
    {
        return (c == '/');
    }

    bool is_space_separator(char c)
    {
        return (c == ' ')
            || (c == '\t');
    }

    bool is_token_separator(char c)
    {
        return is_space_separator(c)
            || (c == '\n');
    }

    template<typename Iterator>
    Iterator skip_lines(Iterator iterator, Iterator last)
    {
        for (; iterator != last;)
        {
            auto character = *iterator;
            switch(character)
            {
                case '\n':
                    return ++iterator;
                default:
                    ++iterator;
            }
        }
        return iterator;
    }

    template<typename Iterator>
    Iterator skip_spaces(Iterator iterator, Iterator last)
    {
        for (; (iterator != last); ++iterator)
        {
            auto character = *iterator;
            if (!is_space_separator(character))
                break;
        }
        return iterator;
    }

    template<typename Iterator>
    std::tuple<Iterator, std::string> parse_token(Iterator iterator, Iterator last)
    {
        std::basic_string<typename Iterator::value_type> v;
        v.reserve(6);
        iterator = skip_spaces(iterator, last);
        for (; (iterator != last); ++iterator)
        {
            auto character = *iterator;
            if (is_token_separator(character))
                break;
            v.push_back(character);
        }
        return std::make_tuple(iterator, v);
    }

    template<typename Iterator>
    std::tuple<Iterator, std::string, face_attribute_t> parse_face_attributes(Iterator iterator, Iterator last, face_attribute_t attribute)
    {
        face_attribute_t next_attribute;
        std::basic_string<typename Iterator::value_type> v;
        v.reserve(6);
        iterator = skip_spaces(iterator, last);
        for (; (iterator != last); ++iterator)
        {
            auto character = *iterator;
            if (is_face_attribute_separator(character))
            {
                switch (attribute)
                {
                    case face_attribute_t::Vertex  : next_attribute = face_attribute_t::TexCoord;
                    case face_attribute_t::TexCoord: next_attribute = face_attribute_t::Normal;
                    case face_attribute_t::Normal  : next_attribute = face_attribute_t::Vertex;
                    default:
                        throw std::runtime_error("Unexpected face attribute: " + std::to_string((int)attribute));
                }
                break;
            }
            else if (is_token_separator(character))
            {
                next_attribute = face_attribute_t::Vertex;
                break;
            }
            v.push_back(character);
        }
        return std::make_tuple(iterator, v, next_attribute);
    }

    template<typename Iterator>
    obj_t process_obj_stream(const fs::path& folderpath, Iterator iterator, Iterator last)
    {
        obj_t obj;
        obj.vertices .reserve(256);
        obj.texcoords.reserve(256);
        obj.normals  .reserve(256);
        obj.points   .reserve(256);

        std::string token;
        std::size_t line = 1;
        int current_group_index = -1;
        std::size_t current_point_index = ~0;

        auto ensure_at_least_one_group = [&obj, &current_group_index]{
            if (obj.groups.empty())
            {
                assert(current_group_index == -1);

                group_t new_group;
                new_group.faces.reserve(256);

                obj.groups.push_back(new_group);
                current_group_index = 0;
            }
        };

        typename Iterator::value_type character;
        for (; iterator != last;)
        {
            character = *iterator;
            switch (character)
            {
                case '\n':
                    {
                        ++iterator;
                        ++line;
                    }
                    break;
                case ' ':
                case '\t':
                    {
                        iterator = skip_spaces(iterator, last);
                    }
                    break;
                case '#':
                    {
                        iterator = skip_lines(iterator, last);
                        ++line;
                    }
                    break;
                case 'm':
                    {
                        std::tie(iterator, token) = parse_token(iterator, last);
                        assert(token == "mtllib");

                        std::tie(iterator, token) = parse_token(iterator, last);
                        obj.material_library = folderpath / token;
                        assert(fs::exists(obj.material_library));
                    }
                    break;
                case 'u':
                    {
                        // FIXME We simply override the current group material
                        //  Instead we need to allocate a new group of faces and assign material to it
                        std::tie(iterator, token) = parse_token(iterator, last);
                        assert(token == "usemtl");

                        ensure_at_least_one_group();

                        group_t& current_group = obj.groups[current_group_index];

                        std::tie(iterator, token) = parse_token(iterator, last);
                        current_group.material = token;
                    }
                    break;
                case 'v':
                    {
                        std::tie(iterator, token) = parse_token(iterator, last);

                        float x, y, z;
                        std::from_chars_result status;

                        assert(!token.empty());
                        if (token.at(0) == 'v')
                        {
                            std::tie(iterator, token) = parse_token(iterator, last);
                            status = std::from_chars(token.c_str(), token.c_str() + token.size(), x);
                            assert(status.ec == std::errc());

                            std::tie(iterator, token) = parse_token(iterator, last);
                            status = std::from_chars(token.c_str(), token.c_str() + token.size(), y);
                            assert(status.ec == std::errc());

                            std::tie(iterator, token) = parse_token(iterator, last);
                            status = std::from_chars(token.c_str(), token.c_str() + token.size(), z);
                            assert(status.ec == std::errc());

                            obj.vertices.push_back({x, y, z});
                        }
                        else if (token == "vt")
                        {
                            std::tie(iterator, token) = parse_token(iterator, last);
                            status = std::from_chars(token.c_str(), token.c_str() + token.size(), x);
                            assert(status.ec == std::errc());

                            std::tie(iterator, token) = parse_token(iterator, last);
                            status = std::from_chars(token.c_str(), token.c_str() + token.size(), y);
                            assert(status.ec == std::errc());

                            obj.texcoords.push_back({x, y});
                        }
                        else if (token == "vn")
                        {
                            std::tie(iterator, token) = parse_token(iterator, last);
                            status = std::from_chars(token.c_str(), token.c_str() + token.size(), x);
                            assert(status.ec == std::errc());

                            std::tie(iterator, token) = parse_token(iterator, last);
                            status = std::from_chars(token.c_str(), token.c_str() + token.size(), y);
                            assert(status.ec == std::errc());

                            std::tie(iterator, token) = parse_token(iterator, last);
                            status = std::from_chars(token.c_str(), token.c_str() + token.size(), z);
                            assert(status.ec == std::errc());

                            obj.normals.push_back({x, y, z});
                        }
                        else
                        {
                            throw std::runtime_error("Unexpected token: " + token);
                        }
                    }
                    break;
                case 'f':
                    {
                        ++iterator;

                        face_t face;

                        // sub loop for parsing points
                        bool parsed = false;

                        int attribute_index;
                        std::from_chars_result status;

                        int face_point_index = -1;
                        face_attribute_t expected_attribute = face_attribute_t::Vertex, next_attribute;

                        for (; !parsed && (iterator != last);)
                        {
                            character = *iterator;
                            switch (character)
                            {
                                case '\n':
                                    {
                                        ++iterator;
                                        ++line;

                                        // NOTE end of the line, face parsing end
                                        ensure_at_least_one_group();
                                        group_t& current_group = obj.groups[current_group_index];

                                        current_group.faces.push_back(face);

                                        parsed = true;
                                    }
                                    break;
                                case ' ':
                                case '\t':
                                    {
                                        iterator = skip_spaces(iterator, last);
                                    }
                                    break;
                                default:
                                    {
                                        std::tie(iterator, token, next_attribute) = parse_face_attributes(iterator, last, expected_attribute);

                                        int attribute_index;
                                        switch(expected_attribute)
                                        {
                                            case face_attribute_t::Vertex:
                                                // Vertex is always the first parsed attribute, start a point encountered
                                                obj.points.push_back(point_t{});

                                                face_point_index += 1;
                                                assert(face_point_index < std::size(face.points));
                                                face.points[face_point_index] = current_point_index = obj.points.size() - 1;

                                                if (!token.empty())
                                                {
                                                    status = std::from_chars(token.c_str(), token.c_str() + token.size(), attribute_index);
                                                    assert(status.ec == std::errc());

                                                    if (attribute_index < 0)
                                                    {
                                                        assert(!obj.vertices.empty());
                                                        attribute_index = obj.vertices.size() + attribute_index;
                                                    }
                                                    assert(attribute_index >= 0);

                                                    point_t& point = obj.points.at(current_point_index);
                                                    point.vertex = static_cast<std::size_t>(attribute_index);
                                                }
                                                break;
                                            case face_attribute_t::TexCoord:
                                                if (!token.empty())
                                                {
                                                    status = std::from_chars(token.c_str(), token.c_str() + token.size(), attribute_index);
                                                    assert(status.ec == std::errc());

                                                    if (attribute_index < 0)
                                                    {
                                                        assert(!obj.texcoords.empty());
                                                        attribute_index = obj.texcoords.size() + attribute_index;
                                                    }
                                                    assert(attribute_index >= 0);

                                                    point_t& point = obj.points.at(current_point_index);
                                                    point.texcoord = static_cast<std::size_t>(attribute_index);
                                                }
                                                break;
                                            case face_attribute_t::Normal:
                                                if (!token.empty())
                                                {
                                                    status = std::from_chars(token.c_str(), token.c_str() + token.size(), attribute_index);
                                                    assert(status.ec == std::errc());

                                                    if (attribute_index < 0)
                                                    {
                                                        assert(!obj.normals.empty());
                                                        attribute_index = obj.normals.size() + attribute_index;
                                                    }
                                                    assert(attribute_index >= 0);

                                                    point_t& point = obj.points.at(current_point_index);
                                                    point.normal = static_cast<std::size_t>(attribute_index);
                                                }
                                                break;
                                            default:
                                                throw std::runtime_error("Unexpected attribute: " + std::to_string((int)expected_attribute));
                                        }

                                        expected_attribute = next_attribute;
                                    }
                                    break;
                            }
                        }
                    }
                    break;
                case 'g':
                    {
                        ++iterator;
                        std::tie(iterator, token) = parse_token(iterator, last);

                        group_t new_group;
                        new_group.name = token;
                        new_group.faces.reserve(256);

                        obj.groups.push_back(new_group);
                        ++current_group_index;
                    }
                    break;
                default:
                    assert(false);
            }
        }

        return obj;
    }
}

int main(int argc, char* argv[])
{
    int inputidx = -1, outputidx = -1;
    for (int idx = 1; idx < argc; ++idx)
    {
        if ((std::strcmp(argv[idx], "-i") == 0) || (std::strcmp(argv[idx], "--input") == 0))
        {
            inputidx = ++idx;
        }
        else  if ((std::strcmp(argv[idx], "-o") == 0) || (std::strcmp(argv[idx], "--output") == 0))
        {
            outputidx = ++idx;
        }
        else if (inputidx == -1)
        {// no input defined yet, pick first positional arg
            inputidx = idx;
        }
        else if (outputidx == -1)
        {// no output defined yet, pick first positional arg
            outputidx = idx;
        }
    }

    if (inputidx == -1)
    {
        std::cerr << "No input given." << std::endl;
        return 1;
    }

    if (outputidx == -1)
    {
        std::cerr << "No output given." << std::endl;
        return 1;
    }

    fs::path input(argv[inputidx]), output(argv[outputidx]);

    std::ifstream istream(input, std::ios::in);
    std::ofstream ostream(output);

    if (!istream.is_open())
    {
        std::cerr << "Failed to open " << input << '.' << std::endl;
        return 2;
    }

    if (!ostream.is_open())
    {
        std::cerr << "Failed to open " << output << '.' << std::endl;
        return 2;
    }

    std::istreambuf_iterator<char> streambegin(istream), streamend;

    obj_t obj = process_obj_stream(input.parent_path(), streambegin, streamend);

    istream.close();

    constexpr std::size_t      kIndexVertex = 0;
    constexpr std::size_t      kIndexPoint = 1;
    std::optional<std::size_t> index_texcoord;
    std::optional<std::size_t> index_normal;

    int shared_attribute_count = 2; // Vertex + Point
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

    std::vector<blk::BufferCPU> obj_mesh_attributes(shared_attribute_count + obj.groups.size());

    static_assert(std::is_same_v<decltype(obj.vertices)::value_type, vertex_t>);
    blk::BufferCPU& attribute_vertices = obj_mesh_attributes.at(kIndexVertex);
    attribute_vertices.pointer        = obj.vertices.data();
    attribute_vertices.count          = obj.vertices.size();
    attribute_vertices.stride         = sizeof(vertex_t);
    attribute_vertices.element_stride = sizeof(float);
    attribute_vertices.usage          = blk::AttributeUsage::Vertex;
    attribute_vertices.datatype       = blk::AttributeDataType::Float;

    static_assert(std::is_same_v<decltype(obj.points)::value_type, point_t>);
    blk::BufferCPU& attribute_points = obj_mesh_attributes.at(kIndexPoint);
    attribute_points.pointer        = obj.points.data();
    attribute_points.count          = obj.points.size();
    attribute_points.stride         = sizeof(point_t);
    attribute_points.element_stride = sizeof(std::size_t);
    attribute_points.usage          = blk::AttributeUsage::Point;
    attribute_points.datatype       = blk::AttributeDataType::Unsigned;

    if (index_texcoord)
    {
        static_assert(std::is_same_v<decltype(obj.texcoords)::value_type, texcoord_t>);
        blk::BufferCPU& attribute_texcoord = obj_mesh_attributes.at(*index_texcoord);
        attribute_texcoord.pointer        = obj.texcoords.data();
        attribute_texcoord.count          = obj.texcoords.size();
        attribute_texcoord.stride         = sizeof(texcoord_t);
        attribute_texcoord.element_stride = sizeof(float);
        attribute_texcoord.usage          = blk::AttributeUsage::TexCoord;
        attribute_texcoord.datatype       = blk::AttributeDataType::Float;
    }

    if (index_normal)
    {
        static_assert(std::is_same_v<decltype(obj.normals)::value_type, normal_t>);
        blk::BufferCPU& attribute_normal = obj_mesh_attributes.at(*index_texcoord);
        attribute_normal.pointer        = obj.normals.data();
        attribute_normal.count          = obj.normals.size();
        attribute_normal.stride         = sizeof(normal_t);
        attribute_normal.element_stride = sizeof(float);
        attribute_normal.usage          = blk::AttributeUsage::Normal;
        attribute_normal.datatype       = blk::AttributeDataType::Float;
    }

    int index_shared_group_face = shared_attribute_count;
    std::vector<blk::MeshCPU> meshes(obj.groups.size());
    for (auto&& [mesh, group] : ranges::views::zip(meshes, obj.groups))
    {
        // assert(!group.faces.empty());
        if (group.faces.empty())
            continue;

        mesh.name = group.name;
        mesh.attributes.resize(shared_attribute_count + 1); // Global Attributes + Faces

        int index_local_group_face = shared_attribute_count;

        static_assert(std::is_same_v<decltype(group.faces)::value_type, face_t>);
        blk::BufferCPU& attribute_face = obj_mesh_attributes.at(index_shared_group_face);
        attribute_face.pointer        = group.faces.data();
        attribute_face.count          = group.faces.size();
        attribute_face.stride         = sizeof(face_t);
        attribute_face.element_stride = sizeof(std::size_t);
        attribute_face.usage          = blk::AttributeUsage::Face;
        attribute_face.datatype       = blk::AttributeDataType::Unsigned;

        mesh.attributes.at(kIndexVertex     ) = std::addressof(obj_mesh_attributes.at(kIndexVertex));
        mesh.attributes.at(kIndexPoint      ) = std::addressof(obj_mesh_attributes.at(kIndexPoint));
        mesh.attributes.at(index_local_group_face) = std::addressof(obj_mesh_attributes.at(index_shared_group_face));
        if (index_texcoord)
            mesh.attributes.at(*index_texcoord) = std::addressof(obj_mesh_attributes.at(*index_texcoord));
        if (index_normal)
            mesh.attributes.at(*index_normal)   = std::addressof(obj_mesh_attributes.at(*index_normal));

        index_shared_group_face += 1;
    }

    std::cout << "Parsed meshes (" << meshes.size() << "):" << std::endl;
    for (auto&& mesh : meshes)
    {
        std::cout << '\t' << "name: " << mesh.name << std::endl;
        std::cout << '\t' << "attributes (" << mesh.attributes.size() << "): " << mesh.name << std::endl;
        for (auto&& attribute : mesh.attributes)
        {
            std::cout << std::endl;
            std::cout << "\t\t" << "pointer        : " << std::hex << attribute->pointer         << std::endl;
            std::cout << "\t\t" << "count          : " << std::dec << attribute->count           << std::endl;
            std::cout << "\t\t" << "stride         : " << std::dec << attribute->stride          << std::endl;
            std::cout << "\t\t" << "element_stride : " << std::dec << attribute->element_stride  << std::endl;
            std::cout << "\t\t" << "element_count  : " << std::dec << attribute->element_count() << std::endl;
            std::cout << "\t\t" << "usage          : " << blk::label(attribute->usage)           << std::endl;
            std::cout << "\t\t" << "datatype       : " << blk::label(attribute->datatype)        << std::endl;
            std::cout << "\t\t" << "buffersize     : " << std::dec << attribute->buffer_size()   << std::endl;

        }
    }

    return EXIT_FAILURE;
}
