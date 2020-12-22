#include "./vkpass.hpp"

#include "./vkdevice.hpp"
#include "./vkrenderpass.hpp"

namespace blk
{
    const blk::Device& Pass::device() const
    {
        return mRenderPass.mDevice;
    }

    Pass::~Pass() = default;
}
