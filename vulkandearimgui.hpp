#pragma once

#include <array>
#include <vector>
#include <cinttypes>
#include <initializer_list>

#include <vulkan/vulkan.h>

class ImGuiContext;

#include "vulkanphysicaldevicebase.hpp"

#include "vulkanbuffer.hpp"

#include "vulkaninstancemixin.hpp"
#include "vulkanphysicaldevicemixin.hpp"
#include "vulkandevicemixin.hpp"
#include "vulkanqueuemixin.hpp"

struct VulkanSurface;

struct VulkanDearImGui
    : public VulkanInstanceMixin<VulkanDearImGui>
    , public VulkanPhysicalDeviceBase
    , public VulkanPhysicalDeviceMixin<VulkanDearImGui>
    , public VulkanDeviceMixin<VulkanDearImGui>
    , public VulkanQueueMixin<VulkanDearImGui>
{
    explicit VulkanDearImGui(
        const VkInstance& instance,
        const VkPhysicalDevice& physical_device,
        const VkDevice& device);
    ~VulkanDearImGui();

    void setup_font();
    void setup_depth(const VkExtent2D& dimension, VkFormat depth_format);
    void setup_queues(std::uint32_t family_index);
    void setup_shaders();
    void setup_descriptors();
    void setup_render_pass(VkFormat color_format, VkFormat depth_format);
    void setup_framebuffers(const VkExtent2D& dimension, const std::vector<VkImageView>& views);
    void setup_graphics_pipeline(const VkExtent2D& dimension);

    void update_imgui_draw_data();

    void new_frame(bool update_frame_times = false);

    void render(VulkanSurface& surface);
    void render_frame(VulkanSurface& surface);
    void build_imgui_command_buffers(VulkanSurface& surface);
    void build_imgui_command_buffers(VulkanSurface& surface, std::uint32_t index);
    void build_imgui_command_buffer(VkCommandBuffer);

    VkInstance       mInstance        = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice  = VK_NULL_HANDLE;
    VkDevice         mDevice          = VK_NULL_HANDLE;
    ImGuiContext*    mContext         = nullptr;

    VkQueue              mQueue                    = VK_NULL_HANDLE;
    VkCommandPool        mCommandPool              = VK_NULL_HANDLE;

    struct
    {
        VkImage        image  = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView    view   = VK_NULL_HANDLE;
    } mDepth;

    struct {
        VkImage        image   = VK_NULL_HANDLE;
        VkDeviceMemory memory  = VK_NULL_HANDLE;
        VkImageView    view    = VK_NULL_HANDLE;
        VkSampler      sampler = VK_NULL_HANDLE;
    } mFont;

    struct Constants {
        float scale    [2];
        float translate[2];
    } mConstantsVertex;

    VkShaderModule             mShaderModuleUIVertex   = VK_NULL_HANDLE;
    VkShaderModule             mShaderModuleUIFragment = VK_NULL_HANDLE;

    VkDescriptorPool           mDescriptorPool         = VK_NULL_HANDLE;
    VkDescriptorSetLayout      mDescriptorSetLayout    = VK_NULL_HANDLE;
    VkDescriptorSet            mDescriptorSet          = VK_NULL_HANDLE;

    VkRenderPass               mRenderPass             = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> mFrameBuffers           ;

    VkPipelineCache            mPipelineCache          = VK_NULL_HANDLE;
    VkPipelineLayout           mPipelineLayout         = VK_NULL_HANDLE;
    VkPipeline                 mPipeline               = VK_NULL_HANDLE;

    // UI & Misc settings
    struct Benchmark {
        // Frame counter to display fps
        std::uint32_t                                  frame_counter = 0;
        std::uint32_t                                  frame_per_seconds = 0;
        float                                          frame_timer = 1.0f;
        std::chrono::high_resolution_clock::time_point frame_tick;
    } mBenchmark;

    struct UI {
        bool                  models          = true;
        bool                  logos           = true;
        bool                  background      = true;
        bool                  animated_lights = false;
        float                 light_speed     = 0.25f;
        float                 light_timer     = 0.0f;
        float                 frame_time_min  = 9999.0f;
        float                 frame_time_max  = 0.0f;
        std::array<float, 50> frame_times;
    } mUI;

    struct Camera {
        float position[3];
        float rotation[3];
    } mCamera;

    struct {
        std::uint32_t  count = 0u;
        VulkanBuffer   buffer;
    } mVertex;
    struct {
        std::uint32_t  count = 0u;
        VulkanBuffer   buffer;
    } mIndex;
};
