#pragma once

#include <vulkan/vulkan_core.h>

#include <cstddef>

#include <span>
#include <tuple>
#include <vector>

#include <chrono>

class VulkanPresentation;

struct VulkanEngine
{
    explicit VulkanEngine(
        VkPhysicalDevice vkphysicaldevice,
        VkDevice vkdevice,
        std::uint32_t queue_family_index,
        const std::span<VkQueue>& vkqueues,
        const VulkanPresentation*
    );
    ~VulkanEngine();

    static
    bool supports(VkPhysicalDevice vkphysicaldevice);

    static
    std::tuple<std::uint32_t, std::uint32_t> requirements(VkPhysicalDevice vkphysicaldevice);

    // Setup

    void recreate_depth(const VkExtent2D& dimension);
    void recreate_framebuffers(const VkExtent2D& dimension, const std::vector<VkImageView>& views);

    // States

    VkPhysicalDevice                     mPhysicalDevice  = VK_NULL_HANDLE;
    VkDevice                             mDevice          = VK_NULL_HANDLE;
    std::uint32_t                        mQueueFamilyIndex;
    VkQueue                              mQueue           = VK_NULL_HANDLE;

    const VulkanPresentation*            mPresentation = nullptr;

    VkPhysicalDeviceFeatures             mFeatures;
    VkPhysicalDeviceProperties           mProperties;
    VkPhysicalDeviceMemoryProperties     mMemoryProperties;

    VkCommandPool                        mCommandPool     = VK_NULL_HANDLE;

    VkFormat                             mDepthFormat;

    struct
    {
        VkImage        image  = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView    view   = VK_NULL_HANDLE;
    } mDepth;

    VkRenderPass               mRenderPass             = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> mFrameBuffers           ;

    VkPipelineCache            mPipelineCache          = VK_NULL_HANDLE;
    VkPipelineLayout           mPipelineLayout         = VK_NULL_HANDLE;
    VkPipeline                 mPipeline               = VK_NULL_HANDLE;

    using clock_type_t = std::chrono::high_resolution_clock;

    // UI & Misc settings
    struct Benchmark {
        // Frame counter to display fps
        std::uint32_t            frame_counter = 0;
        std::uint32_t            frame_per_seconds = 0;
        float                    frame_timer = 1.0f;
        clock_type_t::time_point frame_tick;
    } mBenchmark;
};
