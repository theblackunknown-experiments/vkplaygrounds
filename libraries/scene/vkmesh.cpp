#include "./vkmesh.hpp"

#include "./vkcpumesh.hpp"
#include "./vkgpumesh.hpp"

#include <vkdebug.hpp>

#include <vkdevice.hpp>
#include <vkmemory.hpp>

#include <vkbuffer.hpp>

#include <vulkan/vulkan_core.h>

#include <cassert>

#include <limits>

namespace blk
{

void upload_host_visible(const blk::Device& vkdevice, const CPUMesh& cpumesh, blk::Buffer& vertex_buffer)
{
	assert(vertex_buffer.mMemory);

	blk::Memory& memory = *(vertex_buffer.mMemory);
	assert(memory.mType->supports(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	assert(memory.mType->supports(vertex_buffer));

	VkDeviceSize vertex_size = cpumesh.mVertices.size() * sizeof(Vertex);
	assert(vertex_size <= memory.mFree);

	{ // NOTE We store interleaved vertex attributes
		void* data;
		CHECK(vkMapMemory(vkdevice, memory, vertex_buffer.mOffset, vertex_size, 0, &data));
		std::memcpy(data, cpumesh.mVertices.data(), vertex_size);
		vkUnmapMemory(vkdevice, memory);
	}
}

} // namespace blk
