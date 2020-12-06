#include "./vkbuffer.hpp"

#include "./vkmemory.hpp"

namespace blk
{
    void Buffer::destroy()
    {
        if (mBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(mDevice, mBuffer, nullptr);
            mBuffer   = VK_NULL_HANDLE;
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
