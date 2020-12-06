#include "./vkimage.hpp"

#include "./vkmemory.hpp"

namespace blk
{
    void Image::destroy()
    {
        if (mImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(mDevice, mImage, nullptr);
            mImage   = VK_NULL_HANDLE;
            mOffset   = ~0;
            mOccupied = ~0;

            if (mMemory)
            {
                // FIXME We Assume that this is always the last resource which is unbind from memory
                mMemory->mNextOffset -= mRequirements.size;
                mMemory->mFree       += mRequirements.size;
            }
            mMemory   = nullptr;
        }
    }
}
