
#include <vkmesh.hpp>

#include <cstdlib>
#include <cassert>

#include <charconv>

#include <memory>
#include <string>
#include <iterator>
#include <optional>
#include <unordered_map>

#include <filesystem>

#include <obj2mesh_export.h>

namespace fs = std::filesystem;

namespace blk::meshes::obj
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
        bool                smooth = false;
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

    inline
    bool is_face_attribute_separator(char c)
    {
        return (c == '/');
    }

    inline
    bool is_space_separator(char c)
    {
        return (c == ' ')
            || (c == '\t');
    }

    inline
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
                ++iterator;
                switch (attribute)
                {
                    case face_attribute_t::Vertex  :
                        next_attribute = face_attribute_t::TexCoord;
                        break;
                    case face_attribute_t::TexCoord:
                        next_attribute = face_attribute_t::Normal;
                        break;
                    case face_attribute_t::Normal  :
                        next_attribute = face_attribute_t::Vertex;
                        break;
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
    obj_t parse_obj_stream(const fs::path& folderpath, Iterator iterator, Iterator last)
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
                case 's':
                    {
                        ++iterator;

                        std::tie(iterator, token) = parse_token(iterator, last);

                        ensure_at_least_one_group();

                        group_t& current_group = obj.groups[current_group_index];

                        if (token == "1")
                        {
                            current_group.smooth = true;
                        }
                        else if (token == "off")
                        {
                            current_group.smooth = false;
                        }
                        else
                        {
                            throw std::runtime_error("Unexpected token for smoothing group: " + token);
                        }
                    }
                    break;
                case 'v':
                    {
                        std::tie(iterator, token) = parse_token(iterator, last);

                        float x, y, z;
                        std::from_chars_result status;

                        assert(!token.empty());
                        if (token == "v")
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

                            iterator = skip_spaces(iterator, last);
                            if ((*iterator) != '\n')
                            {
                                std::tie(iterator, token) = parse_token(iterator, last);
                                status = std::from_chars(token.c_str(), token.c_str() + token.size(), z);
                                assert(status.ec == std::errc());
                            }
                            else
                            {
                                z = 0.0f;
                            }

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
                                                if (!token.empty())
                                                {
                                                    // Vertex is always the first parsed attribute, start a point encountered
                                                    obj.points.push_back(point_t{});

                                                    face_point_index += 1;
                                                    assert(face_point_index < std::size(face.points));
                                                    face.points[face_point_index] = current_point_index = obj.points.size() - 1;

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

    struct result_t
    {
        blk::BufferCPU                                  vertices;
        blk::BufferCPU                                  points;
        std::optional<blk::BufferCPU>                   texcoords;
        std::optional<blk::BufferCPU>                   normals;
        std::unordered_map<std::string, blk::BufferCPU> faces_by_group;
    };

    OBJ2MESH_EXPORT
    result_t wrap_as_buffers(const obj_t& obj);
}
