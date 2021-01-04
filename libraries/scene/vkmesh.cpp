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

GPUMesh upload_host_visible(const blk::Device& vkdevice, const CPUMesh& mesh, blk::Memory& memory)
{
	assert(memory.mType.supports(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

	VkDeviceSize vertex_size = mesh.mVertices.size() * sizeof(Vertex);
	assert(vertex_size <= memory.mFree);

	blk::Buffer vertex_buffer(vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	assert(memory.mType.supports(vertex_buffer));

	vertex_buffer.create(vkdevice);

	memory.bind(vertex_buffer);

	{ // NOTE We store interleaved vertex attributes
		void* data;
		CHECK(vkMapMemory(vkdevice, memory, vertex_buffer.mOffset, vertex_size, 0, &data));
		std::memcpy(data, mesh.mVertices.data(), vertex_size);
		vkUnmapMemory(vkdevice, memory);
	}

	return GPUMesh{.mVertexCount = mesh.mVertices.size(), .mVertexBuffer = std::move(vertex_buffer)};
}

} // namespace blk
