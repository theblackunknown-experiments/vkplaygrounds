#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

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

    void create_swapchain();

    void bind_resources();

    // void reallocate_buffers();

    void update_descriptorset();

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

    VkShaderModule                       mShaderModuleUIVertex         = VK_NULL_HANDLE;
    VkShaderModule                       mShaderModuleUIFragment       = VK_NULL_HANDLE;

    VkDescriptorSetLayout                mDescriptorSetLayout          = VK_NULL_HANDLE;
    VkPipelineLayout                     mPipelineLayout               = VK_NULL_HANDLE;

    VkPipelineCache                      mPipelineCache                = VK_NULL_HANDLE;

    VkCommandPool                        mCommandPool                  = VK_NULL_HANDLE;
    VkDescriptorPool                     mDescriptorPool               = VK_NULL_HANDLE;

    // NOTE To scale, we would need to have an array of semaphore present/complete if we want to process frame as fast as possible
    VkSemaphore                          mSemaphorePresentComplete     = VK_NULL_HANDLE;
    VkSemaphore                          mSemaphoreRenderComplete      = VK_NULL_HANDLE;

    ImageData                            mFont;
    ImageData                            mDepth;

    BufferData                           mVertexBuffer, mIndexBuffer;

    std::vector<MemoryData>              mMemoryChunks;

    PresentationData                     mPresentation;

    VkDescriptorSet                      mDescriptorSet                = VK_NULL_HANDLE;
};
