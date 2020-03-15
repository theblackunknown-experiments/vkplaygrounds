#pragma once

#include <vector>
#include <initializer_list>

#include <vulkan/vulkan.h>

class ImGuiContext;

#include "vulkanphysicaldevicebase.hpp"

#include "vulkaninstancemixin.hpp"
#include "vulkanphysicaldevicemixin.hpp"
#include "vulkandevicemixin.hpp"
#include "vulkanqueuemixin.hpp"

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

    void render_frame();

    VkInstance       mInstance        = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice  = VK_NULL_HANDLE;
    VkDevice         mDevice          = VK_NULL_HANDLE;
    ImGuiContext*    mContext         = nullptr;

    VkSemaphore          mSemaphorePresentComplete = VK_NULL_HANDLE;
    VkSemaphore          mSemaphoreRenderComplete  = VK_NULL_HANDLE;
    VkSubmitInfo         mInfoSubmission;
    VkPipelineStageFlags mPipelineStageSubmission  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

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

    struct {
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
};
