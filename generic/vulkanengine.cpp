#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <type_traits>

#include <iostream>

#include "./vulkandebug.hpp"
#include "./vulkanutilities.hpp"

#include "./vulkanengine.hpp"
#include "./vulkanpresentation.hpp"

bool VulkanEngine::supports(VkPhysicalDevice vkphysicaldevice)
{
    std::uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
    assert(count > 0);

    std::vector<VkQueueFamilyProperties> queue_families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, queue_families.data());

    auto optional_queue_family_index = select_queue_family_index(queue_families, VK_QUEUE_GRAPHICS_BIT);

    return optional_queue_family_index.has_value();
}

std::tuple<std::uint32_t, std::uint32_t> VulkanEngine::requirements(VkPhysicalDevice vkphysicaldevice)
{
    constexpr const std::uint32_t queue_count = 1;
    std::uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
    assert(count > 0);

    std::vector<VkQueueFamilyProperties> queue_families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, queue_families.data());

    auto optional_queue_family_index = select_queue_family_index(queue_families, VK_QUEUE_GRAPHICS_BIT);

    if (optional_queue_family_index.has_value())
        return std::make_tuple(optional_queue_family_index.value(), queue_count);

    std::cerr << "No valid physical device supports VulkanEngine" << std::endl;
    return std::make_tuple(0u, 0u);
}

VulkanEngine::VulkanEngine(
        VkPhysicalDevice vkphysicaldevice,
        VkDevice vkdevice,
        std::uint32_t queue_family_index,
        const std::span<VkQueue>& vkqueues,
        const VulkanPresentation* vkpresentation
    )
    : mPhysicalDevice(vkphysicaldevice)
    , mDevice(vkdevice)
    , mQueueFamilyIndex(queue_family_index)
    , mQueue(vkqueues[0])
    , mPresentation(vkpresentation)
{
    assert(vkqueues.size() == 1);
    vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mFeatures);
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &mProperties);
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);

    {// Command Pool
        VkCommandPoolCreateInfo info_pool{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = mQueueFamilyIndex,
        };
        CHECK(vkCreateCommandPool(mDevice, &info_pool, nullptr, &mCommandPool));
    }
    {// Depth
        constexpr const VkFormat kDEPTH_FORMATS[] = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM
        };

        auto finder = std::find_if(
            std::begin(kDEPTH_FORMATS), std::end(kDEPTH_FORMATS),
            [this](const VkFormat& f) {
                VkFormatProperties properties;
                vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, f, &properties);
                return properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
        );

        assert(finder != std::end(kDEPTH_FORMATS));
        mDepthFormat = *finder;
    }
    {// Render Pass
        const VkAttachmentReference reference_color{
            .attachment = 0,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        const VkAttachmentReference reference_depth{
            .attachment = 1,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        const VkSubpassDescription subpass{
            .flags                   = 0,
            .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount    = 0,
            .pInputAttachments       = nullptr,
            .colorAttachmentCount    = 1,
            .pColorAttachments       = &reference_color,
            .pResolveAttachments     = nullptr,
            .pDepthStencilAttachment = &reference_depth,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments    = nullptr,
        };
        const VkSubpassDependency dependencies[2] = {
            VkSubpassDependency{
                .srcSubpass      = VK_SUBPASS_EXTERNAL,
                .dstSubpass      = 0,
                // .srcStageMask needs to be a part of pWaitDstStageMask in the WSI semaphore.
                .srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
                .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
            VkSubpassDependency{
                .srcSubpass      = 0,
                .dstSubpass      = VK_SUBPASS_EXTERNAL,
                .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
        };
        const VkAttachmentDescription attachments[2] = {
            VkAttachmentDescription{
                .flags          = 0,
                .format         = mPresentation->mColorFormat,
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                // The image will automatically be transitioned from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL for rendering, then out to PRESENT_SRC_KHR at the end.
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                // Presenting images in Vulkan requires a special layout.
                .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            VkAttachmentDescription{
                .flags          = 0,
                .format         = mDepthFormat,
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            },
        };
        const VkRenderPassCreateInfo info{
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .attachmentCount = sizeof(attachments) / sizeof(attachments[0]),
            .pAttachments    = attachments,
            .subpassCount    = 1,
            .pSubpasses      = &subpass,
            .dependencyCount = sizeof(dependencies) / sizeof(dependencies[0]),
            .pDependencies   = dependencies,
        };
        CHECK(vkCreateRenderPass(mDevice, &info, nullptr, &mRenderPass));
    }
}

VulkanEngine::~VulkanEngine()
{
    vkDestroyImageView(mDevice, mDepth.view, nullptr);
    vkDestroyImage(mDevice, mDepth.image, nullptr);
    vkFreeMemory(mDevice, mDepth.memory, nullptr);

    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
}

void VulkanEngine::recreate_depth(const VkExtent2D& extent)
{
    if (mDepth.view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(mDevice, mDepth.view, nullptr);
        mDepth.view = VK_NULL_HANDLE;
    }

    if (mDepth.image != VK_NULL_HANDLE)
    {
        vkDestroyImage(mDevice, mDepth.image, nullptr);
        mDepth.image = VK_NULL_HANDLE;
    }

    if (mDepth.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(mDevice, mDepth.memory, nullptr);
        mDepth.memory = VK_NULL_HANDLE;
    }

    {// Image
        const VkImageCreateInfo info{
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = mDepthFormat,
            .extent                = VkExtent3D{
                .width  = extent.width,
                .height = extent.height,
                .depth  = 1,
            },
            .mipLevels             = 1,
            .arrayLayers           = 1,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = VK_IMAGE_TILING_OPTIMAL,
            .usage                 = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        CHECK(vkCreateImage(mDevice, &info, nullptr, &mDepth.image));
    }
    {// Memory
        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(mDevice, mDepth.image, &requirements);

        auto memory_type_index = get_memory_type(mMemoryProperties, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        assert(memory_type_index.has_value());

        const VkMemoryAllocateInfo info{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = requirements.size,
            .memoryTypeIndex = memory_type_index.value(),
        };
        CHECK(vkAllocateMemory(mDevice, &info, nullptr, &mDepth.memory));
    }
    CHECK(vkBindImageMemory(mDevice, mDepth.image, mDepth.memory, 0u));
    {// View
        VkImageViewCreateInfo info{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .image            = mDepth.image,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = mDepthFormat,
            .components       = VkComponentMapping{
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = VkImageSubresourceRange{
                .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        if(mDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
            info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        CHECK(vkCreateImageView(mDevice, &info, nullptr, &mDepth.view));
    }
}

void VulkanEngine::recreate_framebuffers(const VkExtent2D& dimension, const std::vector<VkImageView>& views)
{
    VkImageView attachments[2];

    // Same depth/stencil for all framebuffer
    attachments[1] = mDepth.view;

    const VkFramebufferCreateInfo info{
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0u,
        .renderPass      = mRenderPass,
        .attachmentCount = 2,
        .pAttachments    = attachments,
        .width           = dimension.width,
        .height          = dimension.height,
        .layers          = 1,
    };
    mFrameBuffers.resize(views.size());

    for (std::uint32_t idx = 0u, count = mFrameBuffers.size(); idx < count; ++idx)
    {
        attachments[0] = views.at(idx);
        CHECK(vkCreateFramebuffer(mDevice, &info, nullptr, &mFrameBuffers.at(idx)));
    }
}
