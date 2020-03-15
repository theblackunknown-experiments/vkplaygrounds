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
    VkCommandBuffer create_command_buffer(VkCommandBufferLevel level)
    {
        const Base& that = static_cast<const Base&>(*this);

        VkCommandBuffer command_buffer;
        const VkCommandBufferAllocateInfo info{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = that.mCommandPool,
            .level              = level,
            .commandBufferCount = 1,
        };
        CHECK(vkAllocateCommandBuffers(that.mDevice, &info, &command_buffer));
        return command_buffer;
    }

    void destroy_command_buffer(VkCommandBuffer command_buffer)
    {
        const Base& that = static_cast<const Base&>(*this);
        vkFreeCommandBuffers(that.mDevice, that.mCommandPool, 1, &command_buffer);
    }

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
