#pragma once

#include <string>
#include <vector>

#include <filesystem>

namespace fs = std::filesystem;

namespace blk::meshviewer
{
    struct SharedData
    {
        bool                     mVisualizeMesh = false;

        std::vector<fs::path>    mMeshPaths;
        std::vector<std::string> mMeshLabels;
        int                      mSelectedMeshIndex;

        struct Mouse
        {
            struct Buttons
            {
                bool left   = false;
                bool middle = false;
                bool right  = false;
            } buttons;
            struct Positions
            {
                float x = 0.0f;
                float y = 0.0f;
            } offset;
            struct Wheel
            {
                float vdelta = 0.0f;
                float hdelta = 0.0f;
            } wheel;
        } mMouse;

        SharedData();
    };
}
