#include "./vklauncher.hpp"

#include <vkdebug.hpp>

#include <cassert>
#include <cinttypes>

#include <algorithm>

#include <array>

#include <iostream>
#include <sstream>

#include <vulkan/vulkan.h>

namespace
{
} // namespace

namespace blk::sample00
{
Launcher::Launcher()
{
	std::uint32_t apiVersion;
	CHECK(vkEnumerateInstanceVersion(&apiVersion));
	// This playground is to experiment with Vulkan 1.2
	assert(apiVersion >= VK_MAKE_VERSION(1, 2, 0));

	// NOTE
	//  - https://www.lunarg.com/new-tutorial-for-vulkan-debug-utilities-extension/
	//  - https://www.lunarg.com/wp-content/uploads/2018/05/Vulkan-Debug-Utils_05_18_v1.pdf
	//  - https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_EXT_debug_utils.html
	//  - https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VK_EXT_debug_report.html
	//  - https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkDebugUtilsMessengerCreateInfoEXT.html
	const VkDebugUtilsMessengerCreateInfoEXT info_debug_stdout{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.pNext = nullptr,
		.flags = 0,
		// clang-format off
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		// clang-format on
		.pfnUserCallback = StandardErrorDebugCallback,
		.pUserData = nullptr,
	};

	const VkDebugUtilsMessengerCreateInfoEXT info_debug_debugger{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.pNext = nullptr,
		.flags = 0,
		// clang-format off
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		// clang-format on
		.pfnUserCallback = DebuggerCallback,
		.pUserData = nullptr,
	};

	{ // Instance
		const VkApplicationInfo info_application{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = nullptr,
			.pApplicationName = "00-sample",
			.applicationVersion = VK_MAKE_VERSION(0, 0, 0),
			.pEngineName = "vkplaygrounds",
			.engineVersion = VK_MAKE_VERSION(0, 0, 0),
			.apiVersion = VK_MAKE_VERSION(1, 2, 0),
		};
		const std::array extensions{
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		};
		const std::array layers{
			"VK_LAYER_KHRONOS_validation",
		};
		const VkInstanceCreateInfo info_instance{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = &info_debug_stdout,
			.flags = 0,
			.pApplicationInfo = &info_application,
			.enabledLayerCount = layers.size(),
			.ppEnabledLayerNames = layers.data(),
			.enabledExtensionCount = extensions.size(),
			.ppEnabledExtensionNames = extensions.data(),
		};
		CHECK(vkCreateInstance(&info_instance, nullptr, &mInstance));
	}
	{
		auto vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT"));
		assert(vkCreateDebugUtilsMessengerEXT);
		CHECK(vkCreateDebugUtilsMessengerEXT(mInstance, &info_debug_stdout, nullptr, &mStandardErrorMessenger));
		CHECK(vkCreateDebugUtilsMessengerEXT(mInstance, &info_debug_debugger, nullptr, &mDebuggerMessenger));
	}
}

Launcher::~Launcher()
{
	{
		auto vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT"));
		assert(vkCreateDebugUtilsMessengerEXT);
		vkDestroyDebugUtilsMessengerEXT(mInstance, mDebuggerMessenger, nullptr);
		vkDestroyDebugUtilsMessengerEXT(mInstance, mStandardErrorMessenger, nullptr);
	}
	vkDestroyInstance(mInstance, nullptr);
}
} // namespace blk::sample00
