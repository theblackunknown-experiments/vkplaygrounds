
#include "./vkmemory.hpp"

#include "./vkbuffer.hpp"
#include "./vkimage.hpp"

#include <cassert>

#include <numeric>
#include <algorithm>

namespace
{
template<typename I, typename S>
VkResult _memory_bind_buffers(blk::Memory& memory, I&& first, S&& last)
{
	assert(memory.mMemory != VK_NULL_HANDLE);

	std::vector<VkBindBufferMemoryInfo> infos;
	infos.reserve(std::distance(first, last));

	const VkDeviceSize required_size =
		std::accumulate(first, last, VkDeviceSize{0}, [](VkDeviceSize current, const blk::Buffer* buffer) {
			return current + buffer->mRequirements.size;
		});
	assert(required_size <= memory.mFree);

	std::transform(first, last, std::back_inserter(infos), [&memory](blk::Buffer* buffer) {
		auto info = VkBindBufferMemoryInfo{
			.sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
			.pNext = nullptr,
			.buffer = buffer->mBuffer,
			.memory = memory.mMemory,
			.memoryOffset = memory.mNextOffset,
		};
		buffer->mMemory = &memory;
		buffer->mOffset = memory.mNextOffset;
		buffer->mOccupied = 0;

		memory.mNextOffset += buffer->mRequirements.size;
		memory.mFree -= buffer->mRequirements.size;

		return info;
	});
	auto result = vkBindBufferMemory2(memory.mDevice, infos.size(), infos.data());
	CHECK(result);

	return result;
}

template<typename I, typename S>
VkResult _memory_bind_images(blk::Memory& memory, I&& first, S&& last)
{
	assert(memory.mMemory != VK_NULL_HANDLE);

	std::vector<VkBindImageMemoryInfo> infos;
	infos.reserve(std::distance(first, last));

	const VkDeviceSize required_size =
		std::accumulate(first, last, VkDeviceSize{0}, [](VkDeviceSize current, const blk::Image* image) {
			return current + image->mRequirements.size;
		});
	assert(required_size <= memory.mFree);

	std::transform(first, last, std::back_inserter(infos), [&memory](blk::Image* image) {
		auto info = VkBindImageMemoryInfo{
			.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
			.pNext = nullptr,
			.image = image->mImage,
			.memory = memory.mMemory,
			.memoryOffset = memory.mNextOffset,
		};
		image->mMemory = &memory;
		image->mOffset = memory.mNextOffset;
		image->mOccupied = 0;

		memory.mNextOffset += image->mRequirements.size;
		memory.mFree -= image->mRequirements.size;

		return info;
	});
	auto result = vkBindImageMemory2(memory.mDevice, infos.size(), infos.data());
	CHECK(result);

	return result;
}
} // namespace
namespace blk
{
const MemoryType* PhysicalDeviceMemories::find_compatible(const Buffer& buffer, VkMemoryPropertyFlags flags) const
{
	for (auto&& type : mTypes)
	{
		if (type.mBits & buffer.mRequirements.memoryTypeBits)
		{
			if (flags)
			{
				if (type.supports(flags))
				{
					return std::addressof(type);
				}
			}
			else
			{
				return std::addressof(type);
			}
		}
	}
	return nullptr;
}

const MemoryType* PhysicalDeviceMemories::find_compatible(const Image& image, VkMemoryPropertyFlags flags) const
{
	for (auto&& type : mTypes)
	{
		if (type.mBits & image.mRequirements.memoryTypeBits)
		{
			if (flags)
			{
				if (type.supports(flags))
				{
					return std::addressof(type);
				}
			}
			else
			{
				return std::addressof(type);
			}
		}
	}
	return nullptr;
}

VkResult Memory::bind(const std::span<Buffer*>& buffers)
{
	return _memory_bind_buffers(*this, std::begin(buffers), std::end(buffers));
}

VkResult Memory::bind(const std::initializer_list<Buffer*>& buffers)
{
	return _memory_bind_buffers(*this, std::begin(buffers), std::end(buffers));
}

VkResult Memory::bind(Buffer& buffer)
{
	assert(mMemory != VK_NULL_HANDLE);

	assert(buffer.mRequirements.size <= mFree);

	const VkBindBufferMemoryInfo info{
		.sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
		.pNext = nullptr,
		.buffer = buffer.mBuffer,
		.memory = mMemory,
		.memoryOffset = mNextOffset,
	};
	buffer.mMemory = this;
	buffer.mOffset = mNextOffset;
	buffer.mOccupied = 0;

	mNextOffset += buffer.mRequirements.size;
	mFree -= buffer.mRequirements.size;

	auto result = vkBindBufferMemory2(mDevice, 1, &info);
	CHECK(result);

	return result;
}

VkResult Memory::bind(const std::span<Image*>& images)
{
	return _memory_bind_images(*this, std::begin(images), std::end(images));
}

VkResult Memory::bind(const std::initializer_list<Image*>& images)
{
	return _memory_bind_images(*this, std::begin(images), std::end(images));
}

VkResult Memory::bind(Image& image)
{
	assert(mMemory != VK_NULL_HANDLE);

	assert(image.mRequirements.size <= mFree);

	const VkBindImageMemoryInfo info{
		.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
		.pNext = nullptr,
		.image = image.mImage,
		.memory = mMemory,
		.memoryOffset = mNextOffset,
	};
	image.mMemory = this;
	image.mOffset = mNextOffset;
	image.mOccupied = 0;

	mNextOffset += image.mRequirements.size;
	mFree -= image.mRequirements.size;

	auto result = vkBindImageMemory2(mDevice, 1, &info);
	CHECK(result);

	return result;
}

void* Memory::map(const Buffer& buffer) const
{
	assert(mDevice != VK_NULL_HANDLE);
	assert(mMemory != VK_NULL_HANDLE);
	assert(this == buffer.mMemory);

	void* data;
	CHECK(vkMapMemory(mDevice, mMemory, buffer.mOffset, buffer.mRequirements.size, 0, &data));

	return data;
}

void Memory::unmap(const Buffer& buffer) const
{
	assert(mDevice != VK_NULL_HANDLE);
	assert(mMemory != VK_NULL_HANDLE);
	assert(this == buffer.mMemory);

	vkUnmapMemory(mDevice, mMemory);
}
} // namespace blk
