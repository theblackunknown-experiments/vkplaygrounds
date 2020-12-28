#include "./vkmesh.hpp"

#include <vulkan/vulkan_core.h>

namespace blk
{
VertexInputDescription Vertex::get_description()
{
	return VertexInputDescription{
		.bindings = {VkVertexInputBindingDescription{
			.binding = 0, .stride = sizeof(blk::Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}},
		.attributes = {
			VkVertexInputAttributeDescription{
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = offsetof(blk::Vertex, position),
			},
			VkVertexInputAttributeDescription{
				.location = 1,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = offsetof(blk::Vertex, normal),
			},
			VkVertexInputAttributeDescription{
				.location = 2,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = offsetof(blk::Vertex, color),
			},
		}};
}
} // namespace blk
