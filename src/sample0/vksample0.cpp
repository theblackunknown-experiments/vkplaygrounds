#include "./vksample0.hpp"

#include "../vkengine.hpp"
#include "../vkdevice.hpp"
#include "../vkphysicaldevice.hpp"

#include "../vkmemory.hpp"

#include <vulkan/vulkan_core.h>

#include <array>

namespace
{
    // NVIDIA - https://developer.nvidia.com/blog/vulkan-dos-donts/
    //  > Prefer using 24 bit depth formats for optimal performance
    //  > Prefer using packed depth/stencil formats. This is a common cause for notable performance differences between an OpenGL and Vulkan implementation.
    constexpr std::array kPreferredDepthFormats{
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D32_SFLOAT,
    };
}

namespace blk::sample0
{
Sample::RenderPass::RenderPass(Engine& vkengine, VkFormat formatColor, VkFormat formatDepth)
    : ::blk::RenderPass(vkengine.mDevice)
{
    // Pass 0 : Draw UI    (write depth)
    // Pass 1 : Draw Scene (read depth, write color)
    //  Depth discarded
    //  Color kept
    //  Transition to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR to optimize transition before presentation
    const std::array attachments{
        VkAttachmentDescription{
            .flags          = 0,
            .format         = formatColor,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
        VkAttachmentDescription{
            .flags          = 0,
            .format         = formatDepth,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        },
    };

    constexpr std::uint32_t kSubpassUI = 0;
    constexpr std::uint32_t kSubpassScene = 1;

    constexpr std::uint32_t kAttachmentColor = 0;
    constexpr std::uint32_t kAttachmentDepth = 1;

    constexpr VkAttachmentReference write_color_reference{
        .attachment = kAttachmentColor,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    constexpr VkAttachmentReference write_stencil_reference{
        .attachment = kAttachmentDepth,
        .layout     = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
    };
    constexpr VkAttachmentReference read_stencil_reference{
        .attachment = kAttachmentDepth,
        .layout     = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
    };
    const/*expr*/ std::array subpasses{
        // Pass 0 : Draw UI    (write stencil, write color)
        VkSubpassDescription{
            .flags                   = 0,
            .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount    = 0,
            .pInputAttachments       = nullptr,
            .colorAttachmentCount    = 1,
            .pColorAttachments       = &write_color_reference,
            .pResolveAttachments     = nullptr,
            .pDepthStencilAttachment = &write_stencil_reference,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments    = nullptr,
        },
        // Pass 1 : Draw Scene (read stencil, write color)
        VkSubpassDescription{
            .flags                   = 0,
            .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount    = 0,
            .pInputAttachments       = nullptr,
            .colorAttachmentCount    = 1,
            .pColorAttachments       = &write_color_reference,
            .pResolveAttachments     = nullptr,
            .pDepthStencilAttachment = &read_stencil_reference,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments    = nullptr,
        },
    };
    const/*expr*/ std::array dependencies{
        VkSubpassDependency{
            .srcSubpass      = kSubpassUI                                  ,
            .dstSubpass      = kSubpassScene                               ,
            .srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT   ,
            .dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT  ,
            .srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT ,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT                 ,
        },
    };
    const VkRenderPassCreateInfo info{
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .attachmentCount = attachments.size(),
        .pAttachments    = attachments.data(),
        .subpassCount    = subpasses.size(),
        .pSubpasses      = subpasses.data(),
        .dependencyCount = dependencies.size(),
        .pDependencies   = dependencies.data(),
    };
    CHECK(create(info));
}

Sample::Sample(blk::Engine& vkengine, VkFormat formatColor, const VkExtent2D& resolution)
    : mEngine(vkengine)
    , mDevice(vkengine.mDevice)

    , mColorFormat(formatColor)
    , mDepthFormat([&vkengine]{
        auto finder = std::ranges::find_if(
            kPreferredDepthFormats,
            [&vkengine](const VkFormat& f) {
                VkFormatProperties properties;
                vkGetPhysicalDeviceFormatProperties(*vkengine.mDevice.mPhysicalDevice, f, &properties);
                return properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
        );

        assert(finder != std::end(kPreferredDepthFormats));
        return *finder;
    }())

    , mResolution(resolution)

    , mRenderPass(vkengine, mColorFormat, mDepthFormat)

    , mMultipass(mRenderPass, PassUIOverlay::Arguments{ vkengine, resolution }, PassScene::Arguments{ vkengine, resolution })
    , mPassUIOverlay(subpass<0>(mMultipass))
    , mPassScene(subpass<1>(mMultipass))

    , mDepthImage(
        VkExtent3D{ .width = mResolution.width, .height = mResolution.height, .depth = 1 },
        VK_IMAGE_TYPE_2D,
        mDepthFormat,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED
    )
{
    {// Resources
        mDepthImage.create(mDevice);
    }
    {// Memories
        auto vkphysicaldevice = *(mDevice.mPhysicalDevice);
        // NOTE(andrea.machizaud) not present on Windows laptop with my NVIDIA card...
        auto memory_type = vkphysicaldevice.mMemories.find_compatible(mDepthImage, 0/*VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT*/);
        assert(memory_type);
        mDepthMemory = std::make_unique<blk::Memory>(*memory_type, mDepthImage.mRequirements.size);
        mDepthMemory->allocate(mDevice);
        mDepthMemory->bind(mDepthImage);
    }
    {// Image Views
        mDepthImageView = blk::ImageView(
            mDepthImage,
            VK_IMAGE_VIEW_TYPE_2D,
            mDepthFormat,
            VK_IMAGE_ASPECT_STENCIL_BIT
        );
        mDepthImageView.create(mDevice);
    }
}

}
