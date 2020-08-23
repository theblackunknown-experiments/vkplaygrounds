#pragma once

#include <vulkan/vulkan_core.h>

#include <cinttypes>

#include <vector>

struct ImGuiContext;

struct DearImGuiShowcase
{
    explicit DearImGuiShowcase(VkInstance vkinstance, VkSurfaceKHR vksurface, VkPhysicalDevice vkphysicaldevice);
    ~DearImGuiShowcase();

    static
    bool isSuitable(VkPhysicalDevice vkphysicaldevice, VkSurfaceKHR vksurface);

    void initialize();
    void initialize_resources();
    void initialize_views();

    VkInstance                           mInstance                     = VK_NULL_HANDLE;
    VkSurfaceKHR                         mSurface                      = VK_NULL_HANDLE;
    VkPhysicalDevice                     mPhysicalDevice               = VK_NULL_HANDLE;

    VkPhysicalDeviceFeatures             mFeatures;
    VkPhysicalDeviceProperties           mProperties;
    VkPhysicalDeviceMemoryProperties     mMemoryProperties;
    std::vector<VkQueueFamilyProperties> mQueueFamiliesProperties;
    std::vector<VkExtensionProperties>   mExtensions;

    std::uint32_t                        mQueueFamily                        ;
    VkDevice                             mDevice                       = VK_NULL_HANDLE;
    VkQueue                              mQueue                        = VK_NULL_HANDLE;

    VkColorSpaceKHR                      mColorSpace                   = VK_COLOR_SPACE_MAX_ENUM_KHR;
    VkPresentModeKHR                     mPresentMode                  = VK_PRESENT_MODE_MAX_ENUM_KHR;

    VkFormat                             mDepthStencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    VkFormat                             mColorAttachmentFormat        = VK_FORMAT_UNDEFINED;

    VkSampler                            mSampler                      = VK_NULL_HANDLE;

    VkRenderPass                         mRenderPass                   = VK_NULL_HANDLE;

    VkDescriptorSetLayout                mDescriptorSetLayout          = VK_NULL_HANDLE;
    VkPipelineLayout                     mPipelineLayout               = VK_NULL_HANDLE;

    VkPipelineCache                      mPipelineCache                = VK_NULL_HANDLE;

    VkCommandPool                        mCommandPool                  = VK_NULL_HANDLE;
    VkDescriptorPool                     mDescriptorPool               = VK_NULL_HANDLE;
    VkDescriptorSet                      mDescriptorSet                = VK_NULL_HANDLE;

    // NOTE To scale, we would need to have an array of semaphore present/complete if we want to process frame as fast as possible
    VkSemaphore                          mSemaphorePresentComplete     = VK_NULL_HANDLE;
    VkSemaphore                          mSemaphoreRenderComplete      = VK_NULL_HANDLE;

    struct {
        VkImage     image = VK_NULL_HANDLE;
        VkImageView view  = VK_NULL_HANDLE;
    } mFont;

    ImGuiContext*                        mContext                      = nullptr;
};
