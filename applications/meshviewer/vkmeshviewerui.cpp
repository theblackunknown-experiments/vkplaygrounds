#include "./vkmeshviewerui.hpp"

namespace blk::meshviewer
{

UIMeshInformation::UIMeshInformation()
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
