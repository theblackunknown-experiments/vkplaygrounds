#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <array>
#include <vector>

#include "./vkutilities.hpp"

struct ImGuiContext;

struct DearImGuiShowcase
{
    explicit DearImGuiShowcase(
        VkInstance vkinstance,
        VkSurfaceKHR vksurface,
        VkPhysicalDevice vkphysicaldevice,
        const VkExtent2D& resolution);
    ~DearImGuiShowcase();

    static
    bool isSuitable(VkPhysicalDevice vkphysicaldevice, VkSurfaceKHR vksurface);

    void initialize();
    void initialize_resources();
    void initialize_views();

    void allocate_memory();
    void allocate_descriptorset();

    void upload_font_image();

    void create_swapchain();
    void create_framebuffers();
    void create_graphic_pipelines();

    void bind_resources();

    // void reallocate_buffers();

    void update_descriptorset();

    AcquiredPresentationImage acquire();

    void present(const AcquiredPresentationImage& );

    void wait_pending_operations();

    VkInstance                           mInstance                     = VK_NULL_HANDLE;
    VkSurfaceKHR                         mSurface                      = VK_NULL_HANDLE;
    VkPhysicalDevice                     mPhysicalDevice               = VK_NULL_HANDLE;

    VkExtent2D                           mResolution;

    VkPhysicalDeviceFeatures             mFeatures;
    VkPhysicalDeviceProperties           mProperties;
    VkPhysicalDeviceMemoryProperties     mMemoryProperties;
    std::vector<VkQueueFamilyProperties> mQueueFamiliesProperties;
    std::vector<VkExtensionProperties>   mExtensions;

    std::uint32_t                        mQueueFamily                        ;
    VkDevice                             mDevice                       = VK_NULL_HANDLE;
    VkQueue                              mQueue                        = VK_NULL_HANDLE;

    ImGuiContext*                        mContext                      = nullptr;

    VkColorSpaceKHR                      mColorSpace                   = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR                     mPresentMode                  = VK_PRESENT_MODE_MAX_ENUM_KHR;

    VkFormat                             mDepthStencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    VkFormat                             mColorAttachmentFormat        = VK_FORMAT_UNDEFINED;

    VkSampler                            mSampler                      = VK_NULL_HANDLE;

    VkRenderPass                         mRenderPass                   = VK_NULL_HANDLE;

    VkDescriptorSetLayout                mDescriptorSetLayout          = VK_NULL_HANDLE;
    VkPipelineLayout                     mPipelineLayout               = VK_NULL_HANDLE;

    VkPipelineCache                      mPipelineCache                = VK_NULL_HANDLE;

    std::array<VkPipeline, 2>            mPipelines;

    VkCommandPool                        mCommandPool                  = VK_NULL_HANDLE;
    VkCommandPool                        mCommandPoolOneOff            = VK_NULL_HANDLE;
    VkDescriptorPool                     mDescriptorPool               = VK_NULL_HANDLE;

    // NOTE To scale, we would need to have an array of semaphore present/complete if we want to process frame as fast as possible
    VkSemaphore                          mPresentSemaphore     = VK_NULL_HANDLE;
    VkSemaphore                          mRenderSemaphore      = VK_NULL_HANDLE;

    VkDescriptorSet                      mDescriptorSet                = VK_NULL_HANDLE;

    ImageData                            mFont, mDepth;
    BufferData                           mVertexBuffer, mIndexBuffer, mStaging;

    VkFence                              mStagingFence = VK_NULL_HANDLE;
    VkSemaphore                          mStagingSemaphore = VK_NULL_HANDLE;
    VkCommandBuffer                      mStagingCommandBuffer = VK_NULL_HANDLE;

    std::vector<MemoryData>              mMemoryChunks;

    PresentationData                     mPresentation;

    std::vector<VkFramebuffer>           mFrameBuffers;
};
