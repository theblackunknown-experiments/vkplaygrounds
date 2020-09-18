#include "./vkpassuioverlay.hpp"

#include "./vkutilities.hpp"
#include "./vkdebug.hpp"

#include "font.hpp"
#include "ui-shader.hpp"

#include <vulkan/vulkan_core.h>

#include <imgui.h>
#include <utilities.hpp>

#include <string.h>

#include <cassert>
#include <cstddef>
#include <cinttypes>

#include <chrono>
#include <limits>
#include <numeric>
#include <iterator>

#include <array>
#include <vector>
#include <ranges>
#include <variant>
#include <unordered_map>

namespace
{
    constexpr std::uint32_t kAttachmentColor = 0;
    constexpr std::uint32_t kAttachmentDepth = 1;

    constexpr std::uint32_t kVertexInputBindingPosUVColor = 0;
    constexpr std::uint32_t kUIShaderLocationPos   = 0;
    constexpr std::uint32_t kUIShaderLocationUV    = 1;
    constexpr std::uint32_t kUIShaderLocationColor = 2;

    constexpr std::uint32_t kShaderBindingFontTexture = 0;

    constexpr std::uint32_t kStencilMask      = 0xFF;
    constexpr std::uint32_t kStencilReference = 0x01;

    // TODO(andrea.machizaud) use literals...
    // TODO(andrea.machizaud) pre-allocate a reasonable amount for buffers
    constexpr std::size_t kInitialVertexBufferSize = 2 << 20; // 1 Mb
    constexpr std::size_t kInitialIndexBufferSize  = 2 << 20; // 1 Mb

    struct alignas(4) DearImGuiConstants {
        float scale    [2];
        float translate[2];
    };
}

VulkanPassUIOverlay::VulkanPassUIOverlay(
        const blk::Device&     device,
        const blk::RenderPass& renderpass,
        std::uint32_t          subpass,
        const VkExtent2D&      resolution)
    : mDevice(device)
    , mRenderPass(renderpass)
    , mSubpass(subpass)
    , mResolution(resolution)
    , mContext(ImGui::CreateContext())
    , mVertexBuffer(kInitialVertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
    , mIndexBuffer(kInitialIndexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
    , mStagingBuffer(kInitialIndexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
{
    {// Dear ImGui
        IMGUI_CHECKVERSION();
        {// Style
            ImGuiStyle& style = ImGui::GetStyle();
            ImGui::StyleColorsDark(&style);
            // ImGui::StyleColorsClassic(&style);
            // ImGui::StyleColorsLight(&style);
        }
        // TODO Defer at engine setup in case multiple pass uses dearimgui
        {// IO
            ImGuiIO& io = ImGui::GetIO();
            // TODO Defer at new frame
            io.DisplaySize             = ImVec2(mResolution.width, mResolution.height);
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
            io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
            io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
            io.BackendPlatformName = "Win32";
            io.BackendRendererName = "vkplaygrounds";
            {// Font
                ImFontConfig config;
                strncpy_s(config.Name, kFontName, (sizeof(config.Name) / sizeof(config.Name[0])) - 1);
                config.FontDataOwnedByAtlas = false;
                io.Fonts->AddFontFromMemoryTTF(
                    const_cast<unsigned char*>(&kFont[0]), sizeof(kFont),
                    kFontSizePixels,
                    &config
                );

                int width = 0, height = 0;
                unsigned char* data = nullptr;
                io.Fonts->GetTexDataAsAlpha8(&data, &width, &height);

                mFontExtent = VkExtent3D{
                    .width  = static_cast<std::uint32_t>(width),
                    .height = static_cast<std::uint32_t>(height),
                    .depth  = 1,
                };
            }
        }
    }
}

VulkanPassUIOverlay::~VulkanPassUIOverlay()
{
    for(auto&& vkpipeline : mPipelines)
        vkDestroyPipeline(mDevice, vkpipeline, nullptr);

    vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);

    vkDestroySampler(mDevice, mSampler, nullptr);

    ImGui::DestroyContext(mContext);
}

void VulkanPassUIOverlay::initialize_before_memory_bindings()
{
    /*
    {// Formats
        {// Color Attachment
            // Simple case : we target the same format as the presentation capabilities
            std::uint32_t count;
            vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &count, nullptr);
            assert(count > 0);

            std::vector<VkSurfaceFormatKHR> formats(count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &count, formats.data());

            // NOTE(andrea.machizaud) Arbitrary : it happens to be supported on my device
            constexpr VkFormat kPreferredFormat = VK_FORMAT_B8G8R8A8_UNORM;
            if((count == 1) && (formats.front().format == VK_FORMAT_UNDEFINED))
            {
                // NOTE No preferred format, pick what you want
                mColorAttachmentFormat = kPreferredFormat;
                mColorSpace  = formats.front().colorSpace;
            }
            // otherwise looks for our preferred one, or switch back to the 1st available
            else
            {
                auto finder = std::find_if(
                    std::begin(formats), std::end(formats),
                    [kPreferredFormat](const VkSurfaceFormatKHR& f) {
                        return f.format == kPreferredFormat;
                    }
                );

                if(finder != std::end(formats))
                {
                    const VkSurfaceFormatKHR& preferred_format = *finder;
                    mColorAttachmentFormat = preferred_format.format;
                    mColorSpace  = preferred_format.colorSpace;
                }
                else
                {
                    mColorAttachmentFormat = formats.front().format;
                    mColorSpace  = formats.front().colorSpace;
                }
            }
        }
        {// Depth/Stencil Attachment
            // NVIDIA - https://developer.nvidia.com/blog/vulkan-dos-donts/
            //  > Prefer using 24 bit depth formats for optimal performance
            //  > Prefer using packed depth/stencil formats. This is a common cause for notable performance differences between an OpenGL and Vulkan implementation.
            constexpr VkFormat kDEPTH_FORMATS[] = {
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D16_UNORM,
                VK_FORMAT_D32_SFLOAT,
            };

            auto finder = std::find_if(
                std::begin(kDEPTH_FORMATS), std::end(kDEPTH_FORMATS),
                [vkphysicaldevice = mPhysicalDevice](const VkFormat& f) {
                    VkFormatProperties properties;
                    vkGetPhysicalDeviceFormatProperties(vkphysicaldevice, f, &properties);
                    return properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
                }
            );

            assert(finder != std::end(kDEPTH_FORMATS));
            mDepthStencilAttachmentFormat = *finder;
        }
    }
    */
    {// Sampler
        constexpr VkSamplerCreateInfo info{
            .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .magFilter               = VK_FILTER_NEAREST,
            .minFilter               = VK_FILTER_NEAREST,
            .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias              = 0.0f,
            .anisotropyEnable        = VK_FALSE,
            .maxAnisotropy           = 1.0f,
            .compareEnable           = VK_FALSE,
            .compareOp               = VK_COMPARE_OP_NEVER,
            .minLod                  = -1000.0f,
            .maxLod                  = +1000.0f,
            .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE,
        };
        CHECK(vkCreateSampler(mDevice, &info, nullptr, &mSampler));
    }
    {// Pools
        {// Descriptor Pools
            constexpr std::uint32_t kMaxAllocatedSets = 1;
            constexpr std::array kDescriptorPools{
                // 1 sampler : font texture
                VkDescriptorPoolSize{
                    .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                }
            };
            /*constexpr*/static const VkDescriptorPoolCreateInfo info{
                .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext         = nullptr,
                .flags         = 0,
                .maxSets       = kMaxAllocatedSets,
                .poolSizeCount = kDescriptorPools.size(),
                .pPoolSizes    = kDescriptorPools.data(),
            };
            CHECK(vkCreateDescriptorPool(mDevice, &info, nullptr, &mDescriptorPool));
        }
    }
    {// Descriptor Layouts
        // 1 sampler : font texture
        const std::array<VkDescriptorSetLayoutBinding, 1> bindings{
            VkDescriptorSetLayoutBinding{
                .binding            = kShaderBindingFontTexture,
                .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount    = 1,
                .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = &mSampler,
            },
        };
        const VkDescriptorSetLayoutCreateInfo info{
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = 0,
            .bindingCount  = bindings.size(),
            .pBindings     = bindings.data(),
        };
        CHECK(vkCreateDescriptorSetLayout(mDevice, &info, nullptr, &mDescriptorSetLayout));
    }
    {// Pipeline Layouts
        constexpr std::size_t kAlignOf    = alignof(DearImGuiConstants);
        constexpr std::size_t kMaxAlignOf = alignof(std::max_align_t);
        constexpr std::size_t kSizeOf     = sizeof(DearImGuiConstants);

        constexpr std::array kConstantRanges{
            VkPushConstantRange{
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset     = 0,
                .size       = sizeof(DearImGuiConstants),
            },
        };
        const VkPipelineLayoutCreateInfo info{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = 1,
            .pSetLayouts            = &mDescriptorSetLayout,
            .pushConstantRangeCount = kConstantRanges.size(),
            .pPushConstantRanges    = kConstantRanges.data(),
        };
        CHECK(vkCreatePipelineLayout(mDevice, &info, nullptr, &mPipelineLayout));
    }
    {// Descriptor Set
        const VkDescriptorSetAllocateInfo info{
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorPool     = mDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &mDescriptorSetLayout,
        };
        CHECK(vkAllocateDescriptorSets(mDevice, &info, &mDescriptorSet));
    }
    // NVIDIA - https://developer.nvidia.com/blog/vulkan-dos-donts/
    //  > Parallelize command buffer recording, image and buffer creation, descriptor set updates, pipeline creation, and memory allocation / binding. Task graph architecture is a good option which allows sufficient parallelism in terms of draw submission while also respecting resource and command queue dependencies.
    {// Images
        {// Font
            ImGuiIO& io = ImGui::GetIO();
            {
                int width = 0, height = 0;
                unsigned char* data = nullptr;
                io.Fonts->GetTexDataAsAlpha8(&data, &width, &height);

                mFontImage = blk::Image(
                    VkExtent3D{ .width = (std::uint32_t)width, .height = (std::uint32_t)height, .depth = 1 },
                    VK_IMAGE_TYPE_2D,
                    VK_FORMAT_R8_UNORM,
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED
                );
                mFontImage.create(mDevice);
            }
            io.Fonts->SetTexID(&mFontImage);
        }
    }
    {// Buffers
        mVertexBuffer.create(mDevice);
        mIndexBuffer.create(mDevice);
    }
    {// Pipeline Cache
        constexpr VkPipelineCacheCreateInfo info{
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0u,
            // TODO
            .initialDataSize = 0u,
            .pInitialData    = nullptr,
        };
        CHECK(vkCreatePipelineCache(mDevice, &info, nullptr, &mPipelineCache));
    }
    initialize_graphic_pipelines();
}

void VulkanPassUIOverlay::initialize_after_memory_bindings()
{
    {// ImageView
        mFontImageView = blk::ImageView(
            mFontImage,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_FORMAT_R8_UNORM,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        mFontImageView.create(mDevice);
    }
    {// Update DescriptorSet
        const VkDescriptorImageInfo info{
            .sampler     = VK_NULL_HANDLE,
            .imageView   = mFontImageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        const VkWriteDescriptorSet write{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = mDescriptorSet,
            .dstBinding       = kShaderBindingFontTexture,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &info,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        };
        vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);
    }
}

void VulkanPassUIOverlay::initialize_graphic_pipelines()
{
    assert(std::ranges::all_of(mPipelines, [](VkPipeline pipeline){ return pipeline == VK_NULL_HANDLE;}));

    VkShaderModule shader_ui = VK_NULL_HANDLE;
    VkShaderModule shader_triangle = VK_NULL_HANDLE;

    {// Shader - UI
        constexpr VkShaderModuleCreateInfo info{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = kShaderUI.size() * sizeof(std::uint32_t),
            .pCode    = kShaderUI.data(),
        };
        CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &shader_ui));
    }

    const std::array stages{
        VkPipelineShaderStageCreateInfo{
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_VERTEX_BIT,
            .module              = shader_ui,
            .pName               = "ui_main",
            .pSpecializationInfo = nullptr,
        },
        VkPipelineShaderStageCreateInfo{
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module              = shader_ui,
            .pName               = "ui_main",
            .pSpecializationInfo = nullptr,
        },
    };

    constexpr std::array vertexbindings{
        VkVertexInputBindingDescription
        {
            .binding   = kVertexInputBindingPosUVColor,
            .stride    = sizeof(ImDrawVert),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }
    };

    constexpr std::array vertexattributes{
        VkVertexInputAttributeDescription{
            .location = kUIShaderLocationPos,
            .binding  = kVertexInputBindingPosUVColor,
            .format   = VK_FORMAT_R32G32_SFLOAT,
            .offset   = offsetof(ImDrawVert, pos),
        },
        VkVertexInputAttributeDescription{
            .location = kUIShaderLocationUV,
            .binding  = kVertexInputBindingPosUVColor,
            .format   = VK_FORMAT_R32G32_SFLOAT,
            .offset   = offsetof(ImDrawVert, uv),
        },
        VkVertexInputAttributeDescription{
            .location = kUIShaderLocationColor,
            .binding  = kVertexInputBindingPosUVColor,
            .format   = VK_FORMAT_R8G8B8A8_UNORM,
            .offset   = offsetof(ImDrawVert, col),
        },
    };

    static const/*expr*/ VkPipelineVertexInputStateCreateInfo vertexinput{
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                           = 0,
        .vertexBindingDescriptionCount   = vertexbindings.size(),
        .pVertexBindingDescriptions      = vertexbindings.data(),
        .vertexAttributeDescriptionCount = vertexattributes.size(),
        .pVertexAttributeDescriptions    = vertexattributes.data(),
    };

    constexpr VkPipelineInputAssemblyStateCreateInfo assembly{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    constexpr VkPipelineViewportStateCreateInfo viewport{
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .viewportCount = 1,
        .pViewports    = nullptr, // dynamic state
        .scissorCount  = 1,
        .pScissors     = nullptr, // dynamic state
    };

    constexpr VkPipelineRasterizationStateCreateInfo rasterization{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_NONE,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .lineWidth               = 1.0f,
    };

    constexpr VkPipelineMultisampleStateCreateInfo multisample{
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable   = VK_FALSE,
        .minSampleShading      = 0.0f,
        .pSampleMask           = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE,
    };

    constexpr VkPipelineDepthStencilStateCreateInfo depthstencil{
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .depthTestEnable       = VK_FALSE,
        .depthWriteEnable      = VK_FALSE,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_TRUE,
        .front                 = VkStencilOpState{
            .failOp      = VK_STENCIL_OP_KEEP,
            .passOp      = VK_STENCIL_OP_REPLACE,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp   = VK_COMPARE_OP_ALWAYS,
            .compareMask = kStencilMask,
            .writeMask   = kStencilMask,
            .reference   = kStencilReference,
        },
        .back                  = VkStencilOpState{
            .failOp      = VK_STENCIL_OP_KEEP,
            .passOp      = VK_STENCIL_OP_REPLACE,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp   = VK_COMPARE_OP_ALWAYS,
            .compareMask = kStencilMask,
            .writeMask   = kStencilMask,
            .reference   = kStencilReference,
        },
    };

    constexpr std::array colorblendattachments{
        VkPipelineColorBlendAttachmentState{
            .blendEnable         = VK_FALSE,
            .colorWriteMask      =
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT,
        }
    };

    static const/*expr*/ VkPipelineColorBlendStateCreateInfo colorblend{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .logicOpEnable           = VK_FALSE,
        .logicOp                 = VK_LOGIC_OP_CLEAR,
        .attachmentCount         = colorblendattachments.size(),
        .pAttachments            = colorblendattachments.data(),
    };

    constexpr std::array states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    static const/*expr*/ VkPipelineDynamicStateCreateInfo dynamics{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .dynamicStateCount       = states.size(),
        .pDynamicStates          = states.data(),
    };

    const std::array infos{
        VkGraphicsPipelineCreateInfo
        {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stageCount          = stages.size(),
            .pStages             = stages.data(),
            .pVertexInputState   = &vertexinput,
            .pInputAssemblyState = &assembly,
            .pTessellationState  = nullptr,
            .pViewportState      = &viewport,
            .pRasterizationState = &rasterization,
            .pMultisampleState   = &multisample,
            .pDepthStencilState  = &depthstencil,
            .pColorBlendState    = &colorblend,
            .pDynamicState       = &dynamics,
            .layout              = mPipelineLayout,
            .renderPass          = mRenderPass,
            .subpass             = mSubpass,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1,
        }
    };

    CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, infos.size(), infos.data(), nullptr, mPipelines.data()));

    vkDestroyShaderModule(mDevice, shader_ui, nullptr);
    vkDestroyShaderModule(mDevice, shader_triangle, nullptr);
}

void VulkanPassUIOverlay::record_font_image_upload(VkCommandBuffer commandbuffer)
{
    {// Image Barrier VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        const VkImageMemoryBarrier imagebarrier{
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext               = nullptr,
            .srcAccessMask       = 0,
            .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = mFontImage,
            .subresourceRange    = VkImageSubresourceRange{
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        vkCmdPipelineBarrier(
            commandbuffer,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &imagebarrier
        );
    }
    {// Copy Staging Buffer -> Font Image
        const VkBufferImageCopy region{
            .bufferOffset      = 0,
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,
            .imageSubresource  = VkImageSubresourceLayers{
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
            .imageOffset = VkOffset3D{
                .x = 0,
                .y = 0,
                .z = 0,
            },
            .imageExtent = mFontExtent,
        };
        vkCmdCopyBufferToImage(
            commandbuffer,
            mStagingBuffer,
            mFontImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region
        );
    }
    {// Image Barrier VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        const VkImageMemoryBarrier imagebarrier{
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext               = nullptr,
            .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = mFontImage,
            .subresourceRange    = VkImageSubresourceRange{
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            }
        };
        vkCmdPipelineBarrier(
            commandbuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &imagebarrier
        );
    }
}

AcquiredPresentationImage DearImGuiShowcase::acquire()
{
    std::uint32_t index = ~0;
    const VkResult status = vkAcquireNextImageKHR(
        mDevice,
        mPresentation.swapchain,
        std::numeric_limits<std::uint64_t>::max(),
        mAcquiredSemaphore,
        VK_NULL_HANDLE,
        &index
    );
    CHECK(status);
    return AcquiredPresentationImage{ index, mCommandBuffers.at(index) };
}

void DearImGuiShowcase::present(const AcquiredPresentationImage& presentationimage)
{
    const VkPresentInfoKHR info{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext              = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &mRenderSemaphore,
        .swapchainCount     = 1,
        .pSwapchains        = &mPresentation.swapchain,
        .pImageIndices      = &presentationimage.index,
        .pResults           = nullptr,
    };
    const VkResult result_present = vkQueuePresentKHR(mQueue, &info);
    CHECK(result_present);
}

void DearImGuiShowcase::record(AcquiredPresentationImage& presentationimage)
{
    const ImDrawData* data = ImGui::GetDrawData();
    assert(data);
    assert(data->Valid);

    {// ImGUI
        if (data->TotalVtxCount != 0)
        {
            // TODO Check Alignment
            const VkDeviceSize vertexsize = data->TotalVtxCount * sizeof(ImDrawVert);
            const VkDeviceSize indexsize  = data->TotalIdxCount * sizeof(ImDrawIdx);

            assert(vertexsize <= mVertexBuffer.mRequirements.size);
            assert(indexsize  <= mIndexBuffer .mRequirements.size);

            ImDrawVert* address_vertex = nullptr;
            ImDrawIdx*  address_index = nullptr;
            if ( mVertexBuffer.mMemory != mIndexBuffer.mMemory)
            {
                CHECK(vkMapMemory(
                    mDevice,
                    *mVertexBuffer.mMemory,
                    mVertexBuffer.mOffset,
                    vertexsize,
                    0,
                    reinterpret_cast<void**>(&address_vertex)
                ));
                CHECK(vkMapMemory(
                    mDevice,
                    *mIndexBuffer.mMemory,
                    mIndexBuffer.mOffset,
                    indexsize,
                    0,
                    reinterpret_cast<void**>(&address_index)
                ));
                for(auto idx = 0, count = data->CmdListsCount; idx < count; ++idx)
                {
                    const ImDrawList* list = data->CmdLists[idx];
                    std::copy(list->VtxBuffer.Data, std::next(list->VtxBuffer.Data, list->VtxBuffer.Size), address_vertex);
                    std::copy(list->IdxBuffer.Data, std::next(list->IdxBuffer.Data, list->IdxBuffer.Size), address_index);
                    address_vertex += list->VtxBuffer.Size;
                    address_index  += list->IdxBuffer.Size;
                }
                vkUnmapMemory(mDevice, *mIndexBuffer.mMemory);
                vkUnmapMemory(mDevice, *mVertexBuffer.mMemory);
            }
            else
            {
                {// Vertex
                    CHECK(vkMapMemory(
                        mDevice,
                        *mVertexBuffer.mMemory,
                        mVertexBuffer.mOffset,
                        vertexsize,
                        0,
                        reinterpret_cast<void**>(&address_vertex)
                    ));
                    for(auto idx = 0, count = data->CmdListsCount; idx < count; ++idx)
                    {
                        const ImDrawList* list = data->CmdLists[idx];
                        std::copy(list->VtxBuffer.Data, std::next(list->VtxBuffer.Data, list->VtxBuffer.Size), address_vertex);
                        address_vertex += list->VtxBuffer.Size;
                    }
                    vkUnmapMemory(mDevice, *mVertexBuffer.mMemory);
                }
                {// Index
                    CHECK(vkMapMemory(
                        mDevice,
                        *mIndexBuffer.mMemory,
                        mIndexBuffer.mOffset,
                        indexsize,
                        0,
                        reinterpret_cast<void**>(&address_index)
                    ));
                    for(auto idx = 0, count = data->CmdListsCount; idx < count; ++idx)
                    {
                        const ImDrawList* list = data->CmdLists[idx];
                        std::copy(list->IdxBuffer.Data, std::next(list->IdxBuffer.Data, list->IdxBuffer.Size), address_index);
                        address_index  += list->IdxBuffer.Size;
                    }
                    vkUnmapMemory(mDevice, *mIndexBuffer.mMemory);
                }
            }
            mVertexBuffer.mOccupied = vertexsize;
            mIndexBuffer .mOccupied = indexsize;
        }
    }

    const VkExtent2D resolution{
        .width  = static_cast<std::uint32_t>(data->DisplaySize.x * data->FramebufferScale.x),
        .height = static_cast<std::uint32_t>(data->DisplaySize.y * data->FramebufferScale.y),
    };

    const VkViewport viewport{
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = data->DisplaySize.x * data->FramebufferScale.x,
        .height   = data->DisplaySize.y * data->FramebufferScale.y,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkCommandBuffer cmdbuffer = presentationimage.commandbuffer;

    {// Begin
        constexpr VkCommandBufferBeginInfo info{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .pInheritanceInfo = nullptr,
        };
        CHECK(vkBeginCommandBuffer(cmdbuffer, &info));
    }
    {// Render Passes
        { // Subpass 0 ; ImGui
            constexpr std::array kClearValues {
                VkClearValue {
                    .color = VkClearColorValue{
                        .float32 = { 0.2f, 0.2f, 0.2f, 1.0f }
                    },
                },
                VkClearValue {
                    .depthStencil = VkClearDepthStencilValue{
                        .depth   = 0.0f,
                        .stencil = 0,
                    }
                }
            };
            const VkRenderPassBeginInfo info{
                .sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .pNext            = nullptr,
                .renderPass       = mRenderPass,
                .framebuffer      = mFrameBuffers.at(presentationimage.index),
                .renderArea       = VkRect2D{
                    .offset = VkOffset2D{
                        .x = 0,
                        .y = 0,
                    },
                    .extent = mResolution
                },
                .clearValueCount  = kClearValues.size(),
                .pClearValues     = kClearValues.data(),
            };
            vkCmdBeginRenderPass(cmdbuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines.at(0));
            vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mUIPipelineLayout,
                0,
                1, &mDescriptorSet,
                0, nullptr
            );
            // What should we do ?!
            if (resolution.width != 0 && resolution.height != 0)
            {
                assert(viewport.width > 0);
                assert(viewport.height > 0);
                vkCmdSetViewport(cmdbuffer, 0, 1, &viewport);
            }
            {// Constants
                DearImGuiConstants constants{};
                constants.scale    [0] =  2.0f / data->DisplaySize.x;
                constants.scale    [1] =  2.0f / data->DisplaySize.y;
                constants.translate[0] = -1.0f - data->DisplayPos.x * constants.scale[0];
                constants.translate[1] = -1.0f - data->DisplayPos.y * constants.scale[1];
                vkCmdPushConstants(cmdbuffer, mUIPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);
            }
            if (data->TotalVtxCount > 0)
            {// Buffer Bindings
                constexpr VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(cmdbuffer, kVertexInputBindingPosUVColor, 1, &mVertexBuffer.mBuffer, &offset);
                vkCmdBindIndexBuffer(cmdbuffer, mIndexBuffer, offset, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
            }
            {// Draws
                // Utilities to project scissor/clipping rectangles into framebuffer space
                const ImVec2 clip_offset = data->DisplayPos;        // (0,0) unless using multi-viewports
                const ImVec2 clip_scale  = data->FramebufferScale;  // (1,1) unless using retina display which are often (2,2)

                std::uint32_t offset_index = 0;
                std::int32_t  offset_vertex = 0;
                for (auto idx_list = 0, count_list = data->CmdListsCount; idx_list < count_list; ++idx_list)
                {
                    const ImDrawList* list = data->CmdLists[idx_list];
                    for (auto idx_buffer = 0, count_buffer = list->CmdBuffer.Size; idx_buffer < count_buffer; ++idx_buffer)
                    {
                        const ImDrawCmd& command = list->CmdBuffer[idx_buffer];
                        assert(command.UserCallback == nullptr);

                        // Clip Rect in framebuffer space
                        const ImVec4 framebuffer_clip_rectangle(
                            (command.ClipRect.x - clip_offset.x) * clip_scale.x,
                            (command.ClipRect.y - clip_offset.y) * clip_scale.y,
                            (command.ClipRect.z - clip_offset.x) * clip_scale.x,
                            (command.ClipRect.w - clip_offset.y) * clip_scale.y
                        );

                        const bool valid_clip(
                            (framebuffer_clip_rectangle.x < viewport.width ) &&
                            (framebuffer_clip_rectangle.y < viewport.height) &&
                            (framebuffer_clip_rectangle.w >= 0.0           ) &&
                            (framebuffer_clip_rectangle.w >= 0.0           )
                        );
                        if (valid_clip)
                        {
                            const VkRect2D scissors{
                                .offset = VkOffset2D{
                                    .x = std::max(static_cast<std::int32_t>(framebuffer_clip_rectangle.x), 0),
                                    .y = std::max(static_cast<std::int32_t>(framebuffer_clip_rectangle.y), 0),
                                },
                                .extent = VkExtent2D{
                                    .width  = static_cast<std::uint32_t>(framebuffer_clip_rectangle.z - framebuffer_clip_rectangle.x),
                                    .height = static_cast<std::uint32_t>(framebuffer_clip_rectangle.w - framebuffer_clip_rectangle.y),
                                }
                            };

                            vkCmdSetScissor(cmdbuffer, 0, 1, &scissors);
                            vkCmdDrawIndexed(cmdbuffer, command.ElemCount, 1, command.IdxOffset + offset_index, command.VtxOffset + offset_vertex, 0);
                        }
                    }
                    offset_index  += list->IdxBuffer.Size;
                    offset_vertex += list->VtxBuffer.Size;
                }
            }
        }
        {// Subpass 1 - Scene
            vkCmdNextSubpass(cmdbuffer, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines.at(1));
            vkCmdDraw(cmdbuffer, 3, 1, 0, 0);
        }
        // TODO Subpass Scene
        vkCmdEndRenderPass(cmdbuffer);
    }
    CHECK(vkEndCommandBuffer(cmdbuffer));
}

void DearImGuiShowcase::render_frame()
{
    auto tick_start = std::chrono::high_resolution_clock::now();
    {
        {// ImGui
            ImGuiIO& io = ImGui::GetIO();
            io.DeltaTime = mUI.frame_delta.count();

            io.MousePos                           = ImVec2(mMouse.offset.x, mMouse.offset.y);
            io.MouseDown[ImGuiMouseButton_Left  ] = mMouse.buttons.left;
            io.MouseDown[ImGuiMouseButton_Right ] = mMouse.buttons.right;
            io.MouseDown[ImGuiMouseButton_Middle] = mMouse.buttons.middle;

            ImGui::NewFrame();
            {// Window
                if (ImGui::BeginMainMenuBar())
                {
                    if (ImGui::BeginMenu("About"))
                    {
                        ImGui::MenuItem("GPU Information", "", &mUI.show_gpu_information);
                        ImGui::MenuItem("Show Demos", "", &mUI.show_demo);
                        ImGui::EndMenu();
                    }
                    ImGui::EndMainMenuBar();
                }
                // ImGui::Begin(kWindowTitle);
                // ImGui::End();

                if (mUI.show_gpu_information)
                {
                    ImGui::Begin("GPU Information");

                    ImGui::TextUnformatted(mPhysicalDevice.mProperties.deviceName);

                    // TODO Do it outside ImGui frame
                    // Update frame time display
                    if (mUI.frame_count == 0)
                    {
                        std::rotate(
                            std::begin(mUI.frame_times), std::next(std::begin(mUI.frame_times)),
                            std::end(mUI.frame_times)
                        );

                        mUI.frame_times.back() = mUI.frame_delta.count();
                        // std::cout << "Frame times: ";
                        // std::copy(
                        //     std::begin(mUI.frame_times), std::end(mUI.frame_times),
                        //     std::ostream_iterator<float>(std::cout, ", ")
                        // );
                        // std::cout << std::endl;
                        auto [find_min, find_max] = std::minmax_element(std::begin(mUI.frame_times), std::end(mUI.frame_times));
                        mUI.frame_time_min = *find_min;
                        mUI.frame_time_max = *find_max;
                    }

                    ImGui::PlotLines(
                        "Frame Times",
                        mUI.frame_times.data(), mUI.frame_times.size(),
                        0,
                        nullptr,
                        mUI.frame_time_min,
                        mUI.frame_time_max,
                        ImVec2(0, 80)
                    );

                    ImGui::End();
                }
            }
            if (mUI.show_demo)
            {// Demo Window
                ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
                ImGui::ShowDemoWindow();
            }
            ImGui::Render();
        }

        auto presentationimage = acquire();
        record(presentationimage);
        {// Submit
            constexpr VkPipelineStageFlags kFixedWaitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            const std::array<VkSubmitInfo, 1> infos{
                VkSubmitInfo{
                    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .pNext                = nullptr,
                    .waitSemaphoreCount   = 1,
                    .pWaitSemaphores      = &mAcquiredSemaphore,
                    .pWaitDstStageMask    = &kFixedWaitDstStageMask,
                    .commandBufferCount   = 1,
                    .pCommandBuffers      = &presentationimage.commandbuffer,
                    .signalSemaphoreCount = 1,
                    .pSignalSemaphores    = &mRenderSemaphore,
                }
            };
            CHECK(vkQueueSubmit(mQueue, infos.size(), infos.data(), VK_NULL_HANDLE));
        }
        present(presentationimage);

        // NOTE We block to avoid override something in use (e.g. surface indexed VkCommandBuffer)
        // TODO Block per command buffer, instead of a global lock for all of them
        CHECK(vkQueueWaitIdle(mQueue));
    }
    auto tick_end  = std::chrono::high_resolution_clock::now();
    mUI.frame_delta = frame_time_delta_ms_t(tick_end - tick_start);

    ++mUI.frame_count;

    using namespace std::chrono_literals;

    auto elapsed_time = frame_time_delta_s_t(tick_end - mUI.count_tick);
    if (elapsed_time > 1s)
    {
        auto fps = mUI.frame_count * elapsed_time;
        mUI.frame_count = 0;
        mUI.count_tick  = tick_end;
    }
}

void DearImGuiShowcase::wait_pending_operations()
{
    vkWaitForFences(mDevice, 1, &mStagingFence, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
}
