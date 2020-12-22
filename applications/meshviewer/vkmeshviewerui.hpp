#pragma once

#include <string>
#include <vector>

#include <filesystem>

namespace fs = std::filesystem;

namespace blk::meshviewer
{
    struct UIMeshInformation
    {
        std::vector<fs::path>    mMeshPaths;
        std::vector<std::string> mMeshLabels;
        int                      mSelectedMeshIndex;

        UIMeshInformation();
    };
}
