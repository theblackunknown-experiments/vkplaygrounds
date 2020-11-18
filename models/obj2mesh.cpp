
#include <cstdlib>
#include <cassert>

#include <fstream>
#include <iostream>
#include <charconv>

#include <span>
#include <iterator>

#include <filesystem>

namespace fs = std::filesystem;

namespace
{
    struct Result
    {
        struct vertex {
            float x, y, z;
        };

        struct face {
            int v0, v1, v2, v3;
        };

        struct group {
            std::string         name;
            std::string         material;
            std::vector<face>   faces;
        };

        fs::path path_material_library;
        std::vector<vertex> vertices;
        std::vector<group> groups;
    };

    bool is_token_separator(char c)
    {
        return (c == ' ')
            || (c == '\n')
            || (c == '\t');
    }

    template<typename Iterator>
    Iterator skip_lines(Iterator iterator, Iterator last)
    {
        for (auto character = *iterator; iterator != last;)
        {
            switch(character)
            {
                case '\n':
                    return ++iterator;
                default:
                    ++iterator;
                    character = *iterator;
            }
        }
        return iterator;
    }

    template<typename Iterator>
    Iterator skip_spaces(Iterator iterator, Iterator last)
    {
        for (auto character = *iterator; (iterator != last) && (character == ' '); character = *(++iterator))
        {
        }
        return iterator;
    }

    template<typename Iterator>
    std::tuple<Iterator, std::string> parse_token(Iterator iterator, Iterator last)
    {
        std::basic_string<typename Iterator::value_type> v;
        v.reserve(6);
        iterator = skip_spaces(iterator, last);
        for (auto character = *iterator; (iterator != last) && !is_token_separator(character); character = *(++iterator))
        {
            v.push_back(character);
        }
        return std::make_tuple(iterator, v);
    }

    template<typename Iterator>
    Result process_obj_stream(const fs::path& folderpath, Iterator iterator, Iterator last)
    {
        Result r;
        r.vertices.reserve(256);

        std::string token;
        int current_group_index = -1;

        for (auto character = *iterator; iterator != last; character = *iterator)
        {
            switch (character)
            {
                case '\n':
                    {
                        ++iterator;
                    }
                    break;
                case '#':
                    {
                        iterator = skip_lines(iterator, last);
                    }
                    break;
                case ' ':
                case '\t':
                    {
                        iterator = skip_spaces(iterator, last);
                    }
                    break;
                case 'm':
                    {
                        std::tie(iterator, token) = parse_token(iterator, last);
                        assert(token == "mtllib");

                        std::tie(iterator, token) = parse_token(iterator, last);
                        r.path_material_library = folderpath / token;
                        assert(fs::exists(r.path_material_library));
                    }
                    break;
                case 'u':
                    {
                        // FIXME We simply override the current group material
                        //  Instead we need to allocate a new group of faces and assign material to it
                        std::tie(iterator, token) = parse_token(iterator, last);
                        assert(token == "usemtl");

                        if (r.groups.empty())
                        {
                            assert(current_group_index == -1);
                            r.groups.push_back({});
                            current_group_index = 0;
                        }

                        Result::group& current_group = r.groups[current_group_index];

                        std::tie(iterator, token) = parse_token(iterator, last);
                        current_group.material = token;
                    }
                    break;
                case 'v':
                    {
                        ++iterator;

                        float x, y, z;
                        std::from_chars_result status;

                        std::tie(iterator, token) = parse_token(iterator, last);
                        status = std::from_chars(token.c_str(), token.c_str() + token.size(), x);
                        assert(status.ec == std::errc());

                        std::tie(iterator, token) = parse_token(iterator, last);
                        status = std::from_chars(token.c_str(), token.c_str() + token.size(), y);
                        assert(status.ec == std::errc());

                        std::tie(iterator, token) = parse_token(iterator, last);
                        status = std::from_chars(token.c_str(), token.c_str() + token.size(), z);
                        assert(status.ec == std::errc());

                        r.vertices.push_back({x, y, z});
                    }
                    break;
                case 'f':
                    {
                        ++iterator;

                        int v0, v1, v2, v3;
                        std::from_chars_result status;

                        if (r.groups.empty())
                        {
                            assert(current_group_index == -1);
                            r.groups.push_back({});
                            current_group_index = 0;
                        }

                        Result::group& current_group = r.groups[current_group_index];

                        std::tie(iterator, token) = parse_token(iterator, last);
                        status = std::from_chars(token.c_str(), token.c_str() + token.size(), v0);
                        assert(status.ec == std::errc());

                        if (v0 < 0)
                        {
                            assert(!r.vertices.empty());
                            v0 = r.vertices.size() + v0;
                        }

                        std::tie(iterator, token) = parse_token(iterator, last);
                        status = std::from_chars(token.c_str(), token.c_str() + token.size(), v1);
                        assert(status.ec == std::errc());

                        if (v1 < 0)
                        {
                            assert(!r.vertices.empty());
                            v1 = r.vertices.size() + v1;
                        }

                        std::tie(iterator, token) = parse_token(iterator, last);
                        status = std::from_chars(token.c_str(), token.c_str() + token.size(), v2);
                        assert(status.ec == std::errc());

                        if (v2 < 0)
                        {
                            assert(!r.vertices.empty());
                            v2 = r.vertices.size() + v2;
                        }

                        std::tie(iterator, token) = parse_token(iterator, last);
                        status = std::from_chars(token.c_str(), token.c_str() + token.size(), v3);
                        assert(status.ec == std::errc());

                        if (v3 < 0)
                        {
                            assert(!r.vertices.empty());
                            v3 = r.vertices.size() + v3;
                        }
                        current_group.faces.push_back({v0, v1, v2, v3});
                    }
                    break;
                case 'g':
                    {
                        ++iterator;
                        std::tie(iterator, token) = parse_token(iterator, last);

                        Result::group new_group;
                        new_group.name = token;

                        r.groups.push_back(new_group);
                        ++current_group_index;
                    }
                    break;
                default:
                    assert(false);
            }
        }

        return { };
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

    auto result = process_obj_stream(input.parent_path(), streambegin, streamend);

    istream.close();

    return EXIT_FAILURE;
}
