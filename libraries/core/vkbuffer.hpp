#pragma once

#include "./vkdebug.hpp"

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <core_export.h>

namespace blk
{
struct Memory;

struct Buffer
{
	VkBufferCreateInfo mInfo;
	VkBuffer mBuffer = VK_NULL_HANDLE;
	VkDevice mDevice = VK_NULL_HANDLE;
	VkMemoryRequirements mRequirements;

	Memory* mMemory = nullptr;
	std::uint32_t mOffset = ~0;
	std::uint32_t mOccupied = ~0;

	constexpr Buffer() = default;

	constexpr Buffer(const Buffer& rhs) = delete;

	constexpr Buffer(Buffer&& rhs)
		: mInfo(std::move(rhs.mInfo))
		, mBuffer(std::exchange(rhs.mBuffer, VkBuffer{VK_NULL_HANDLE}))
		, mDevice(std::exchange(rhs.mDevice, VkDevice{VK_NULL_HANDLE}))
		, mRequirements(std::move(rhs.mRequirements))
		, mMemory(std::exchange(rhs.mMemory, nullptr))
		, mOffset(std::exchange(rhs.mOffset, ~0))
		, mOccupied(std::exchange(rhs.mOccupied, ~0))
	{
	}

	constexpr explicit Buffer(const VkDeviceSize& size, VkBufferUsageFlags flags)
		: Buffer(VkBufferCreateInfo{
			  .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			  .pNext = nullptr,
			  .flags = 0,
			  .size = size,
			  .usage = flags,
			  .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			  .queueFamilyIndexCount = 0,
			  .pQueueFamilyIndices = nullptr,
		  })
	{
	}

	constexpr explicit Buffer(const VkBufferCreateInfo& info): mInfo(info) {}

	~Buffer() { destroy(); }

	Buffer& operator=(const Buffer& rhs) = delete;

	Buffer& operator=(Buffer&& rhs)
	{
		mInfo = std::exchange(rhs.mInfo, mInfo);
		mBuffer = std::exchange(rhs.mBuffer, mBuffer);
		mDevice = std::exchange(rhs.mDevice, mDevice);
		mRequirements = std::exchange(rhs.mRequirements, mRequirements);
		mMemory = std::exchange(rhs.mMemory, mMemory);
		mOffset = std::exchange(rhs.mOffset, mOffset);
		mOccupied = std::exchange(rhs.mOccupied, mOccupied);
		return *this;
	}

	VkResult create(VkDevice vkdevice)
	{
		mDevice = vkdevice;
		auto result = vkCreateBuffer(mDevice, &mInfo, nullptr, &mBuffer);
		CHECK(result);
		vkGetBufferMemoryRequirements(mDevice, mBuffer, &mRequirements);
		return result;
	}

	CORE_EXPORT void destroy();

	constexpr bool created() const { return mBuffer != VK_NULL_HANDLE; }

	constexpr bool bound() const { return mOffset != (std::uint32_t)~0; }

	constexpr operator VkBuffer() const { return mBuffer; }
};

enum class AttributeUsage
{
	Generic,
	Vertex,
	TexCoord,
	Normal,
	IndexedVertex,
	IndexedTexCoord,
	IndexedNormal,
	IndexedVertexTexCoordNormal,

	Point = IndexedVertexTexCoordNormal,
	IndexedPoint,

	Face = IndexedPoint,
};

inline constexpr const char* label(AttributeUsage usage) noexcept
{
	switch (usage)
	{
	case AttributeUsage::Generic: return "Generic";
	case AttributeUsage::Vertex: return "Vertex";
	case AttributeUsage::TexCoord: return "TexCoord";
	case AttributeUsage::Normal: return "Normal";
	case AttributeUsage::IndexedVertex: return "IndexedVertex";
	case AttributeUsage::IndexedTexCoord: return "IndexedTexCoord";
	case AttributeUsage::IndexedNormal: return "IndexedNormal";
	case AttributeUsage::IndexedVertexTexCoordNormal: return "Point";
	case AttributeUsage::IndexedPoint: return "Face";
	default: return "UNKNOWN";
	}
}

enum class AttributeDataType
{
	Float,
	Unsigned,
};

inline constexpr const char* label(AttributeDataType usage) noexcept
{
	switch (usage)
	{
	case AttributeDataType::Float: return "Float";
	case AttributeDataType::Unsigned: return "Unsigned";
	default: return "UNKNOWN";
	}
}

struct BufferCPU
{
	const void* pointer;          // Where does the attribute starts in storage
	std::uint64_t count;          // Number of attributes
	std::uint32_t stride;         // Size in bytes between two consecutives Attribute in storage
	std::uint32_t element_stride; // Size in bytes between two consecutives Attribute element in storage
	AttributeUsage usage;
	AttributeDataType datatype;

	constexpr std::uint32_t element_count() const noexcept { return stride / element_stride; }

	constexpr std::uint64_t buffer_size() const noexcept { return count * stride; }
};

} // namespace blk
