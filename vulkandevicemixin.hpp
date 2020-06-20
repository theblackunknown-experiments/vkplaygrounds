#pragma once

#include <vulkan/vulkan.h>

#include <vector>

// TODO Port to .cpp file
#include <ios>
#include <cstddef>
#include <fstream>
#include <cinttypes>

template<typename Base>
struct VulkanDeviceMixin
{
    [[nodiscard]]
    VkCommandBuffer create_command_buffer(VkCommandPool cmdpool, VkCommandBufferLevel level)
    {
        const Base& that = static_cast<const Base&>(*this);

        VkCommandBuffer command_buffer;
        const VkCommandBufferAllocateInfo info{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = cmdpool,
            .level              = level,
            .commandBufferCount = 1,
        };
        CHECK(vkAllocateCommandBuffers(that.mDevice, &info, &command_buffer));
        return command_buffer;
    }

    void destroy_command_buffer(VkCommandPool cmdpool, VkCommandBuffer command_buffer)
    {
        const Base& that = static_cast<const Base&>(*this);
        vkFreeCommandBuffers(that.mDevice, cmdpool, 1, &command_buffer);
    }

    VkResult submit_command_buffer(VkQueue queue, VkCommandBuffer command_buffer)
    {
        const Base& that = static_cast<const Base&>(*this);
        const VkSubmitInfo info_submit{
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 0,
            .pWaitSemaphores      = nullptr,
            .pWaitDstStageMask    = nullptr,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &command_buffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores    = nullptr,
        };
        const VkFenceCreateInfo info_fence{
            .sType              = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext              = nullptr,
            .flags              = 0,
        };

        VkFence fence;
        CHECK(vkCreateFence(that.mDevice, &info_fence, nullptr, &fence));

        VkResult result = vkQueueSubmit(queue, 1, &info_submit, fence);

        CHECK(vkWaitForFences(that.mDevice, 1, &fence, VK_TRUE, 10e9));
        vkDestroyFence(that.mDevice, fence, nullptr);

        return result;
    }

    [[nodiscard]]
    VkShaderModule load_shader(const std::filesystem::path& filepath)
    {
        const Base& that = static_cast<const Base&>(*this);

        std::ifstream stream(filepath, std::ios::binary | std::ios::in | std::ios::ate);
        assert(stream.is_open());

        auto size = stream.tellg();
        stream.seekg(0, std::ios_base::beg);

        std::vector<std::byte> data(size);
        stream.close();

        assert(size > 0);

        const VkShaderModuleCreateInfo info{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = static_cast<std::size_t>(size),
            .pCode    = reinterpret_cast<std::uint32_t*>(data.data()),
        };
        VkShaderModule module;
        CHECK(vkCreateShaderModule(that.mDevice, &info, nullptr, &module));
        return module;
    }
};
