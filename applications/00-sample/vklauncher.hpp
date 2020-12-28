#pragma once

#include <vulkan/vulkan_core.h>

namespace blk::sample00
{
struct Launcher
{
	explicit Launcher();
	~Launcher();

	operator VkInstance() const { return mInstance; }

	VkInstance mInstance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT mStandardErrorMessenger = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT mDebuggerMessenger = VK_NULL_HANDLE;
};
} // namespace blk::sample00
