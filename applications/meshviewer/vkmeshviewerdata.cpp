#include "./vkmeshviewerdata.hpp"

namespace blk::meshviewer
{

SharedData::SharedData()
    : mMeshPaths{
        fs::path("C:/devel/vkplaygrounds/data/models/CornellBox/CornellBox-Original.obj")
    }
    , mMeshLabels{
        "Cornell Box (Original)"
    }
    , mSelectedMeshIndex(0)
{
}

}