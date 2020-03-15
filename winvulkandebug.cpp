#include <windows.h>

#include <vulkan/vulkan.h>

#include <cassert>

#include "vulkandebug.hpp"

void CHECK(const VkResult& v)
{
    if (!(v == VK_SUCCESS) && IsDebuggerPresent())
    {
        DebugBreak();
    }
    assert(v == VK_SUCCESS);
}
