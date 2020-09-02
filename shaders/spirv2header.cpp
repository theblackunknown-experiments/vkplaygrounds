
#include <cstdlib>

#include <fstream>
#include <iostream>

#include <span>
#include <iterator>

#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
    int inputidx = -1, outputidx = -1, variableidx = -1;
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
        else  if ((std::strcmp(argv[idx], "-vn") == 0) || (std::strcmp(argv[idx], "--variable-name") == 0))
        {
            variableidx = ++idx;
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

    if (variableidx == -1)
    {
        std::cerr << "No variable name given." << std::endl;
        return 1;
    }

    const std::string variable(argv[variableidx]);
    fs::path input(argv[inputidx]), output(argv[outputidx]);

    std::ifstream istream(input, std::ios::binary);
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
    std::vector<char> buffer(streambegin, streamend);
    istream.close();

    if (buffer.empty())
    {
        std::cerr << "Failed to extract an std::uint32_t from " << input << '.' << std::endl;
        fs::remove(output);
        return 3;
    }

    const std::span<std::uint32_t> shadercode(reinterpret_cast<std::uint32_t*>(buffer.data()), buffer.size() / sizeof(std::uint32_t));

    ostream <<
R"__(
#pragma once

#include <cinttypes>

#include <span>
#include <array>

)__";

    auto it = std::begin(shadercode), end = std::end(shadercode);
    ostream << "constexpr const std::array<std::uint32_t, " << shadercode.size() << "> " << variable << " {" << std::endl;

    ostream << std::showbase << std::hex << *it;
    ++it;
    for (; it != end; ++it)
    {
       ostream << ", " << std::showbase << std::hex << *it;
    }

    ostream <<
R"__(
};

)__";

    return EXIT_SUCCESS;
}
