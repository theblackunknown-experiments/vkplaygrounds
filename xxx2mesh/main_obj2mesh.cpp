
#include <obj2mesh.hpp>
#include <vkmesh.hpp>

#include <fstream>
#include <iostream>
#include <type_traits>

// #include <memory>
#include <iterator>
#include <optional>

#include <filesystem>

#include <range/v3/view/zip.hpp>

namespace fs = std::filesystem;

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

    // if (outputidx == -1)
    // {
    //     std::cerr << "No output given." << std::endl;
    //     return 1;
    // }

    fs::path input(argv[inputidx]);
    // fs::path output(argv[outputidx]);

    std::ifstream istream(input, std::ios::in);
    // std::ofstream ostream(output);

    if (!istream.is_open())
    {
        std::cerr << "Failed to open " << input << '.' << std::endl;
        return 2;
    }

    // if (!ostream.is_open())
    // {
    //     std::cerr << "Failed to open " << output << '.' << std::endl;
    //     return 2;
    // }

    std::istreambuf_iterator<char> streambegin(istream), streamend;

    const blk::meshes::obj::obj_t obj = blk::meshes::obj::parse_obj_stream(input.parent_path(), streambegin, streamend);

    istream.close();

    const blk::meshes::obj::result_t result = blk::meshes::obj::wrap_as_buffers(obj);
    std::cout << "Parsed meshes (" << result.faces_by_group.size() << "):" << std::endl;
    for (auto&& [group_name, faces] : result.faces_by_group)
    {
        std::cout << "name: " << group_name << std::endl;
        std::cout << '\t' << faces.count << " face(s) " << std::endl;
    }

    return EXIT_FAILURE;
}
