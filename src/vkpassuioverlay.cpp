#include "./vkpassuioverlay.hpp"

#include "./vkutilities.hpp"
#include "./vkdebug.hpp"

#include "./vkphysicaldevice.hpp"

#include "font.hpp"
#include "ui-shader.hpp"

#include <vulkan/vulkan_core.h>

#include <imgui.h>
#include <utilities.hpp>

#include <string.h>

#include <cassert>
#include <cinttypes>

#include <chrono>
#include <limits>
#include <numeric>
#include <iterator>

#include <array>
#include <vector>
#include <ranges>

namespace
{
    constexpr std::uint32_t kVertexInputBindingPosUVColor = 0;
    constexpr std::uint32_t kUIShaderLocationPos          = 0;
    constexpr std::uint32_t kUIShaderLocationUV           = 1;
    constexpr std::uint32_t kUIShaderLocationColor        = 2;

    constexpr std::uint32_t kShaderBindingFontTexture     = 0;

    constexpr std::uint32_t kStencilMask                  = 0xFF;
    constexpr std::uint32_t kStencilReference             = 0x01;

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

void VulkanPassUIOverlay::render_frame()
{
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

                    ImGui::TextUnformatted(mDevice.mPhysicalDevice->mProperties.deviceName);

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
    }
}

void VulkanPassUIOverlay::upload_frame_data()
{
    const ImDrawData* data = ImGui::GetDrawData();
    assert(data);
    assert(data->Valid);

    if (data->TotalVtxCount != 0)
    {
        // TODO Check Alignment
        const VkDeviceSize vertexsize = data->TotalVtxCount * sizeof(ImDrawVert);
        const VkDeviceSize indexsize  = data->TotalIdxCount * sizeof(ImDrawIdx);

        assert(vertexsize <= mVertexBuffer.mRequirements.size);
        assert(indexsize  <= mIndexBuffer .mRequirements.size);

        ImDrawVert* address_vertex = nullptr;
        ImDrawIdx*  address_index = nullptr;
        if (mVertexBuffer.mMemory != mIndexBuffer.mMemory)
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
    }
    else
    {
        mVertexBuffer.mOccupied = 0;
        mIndexBuffer .mOccupied = 0;
    }
}

void VulkanPassUIOverlay::record_frame(VkCommandBuffer commandbuffer)
{
    const ImDrawData* data = ImGui::GetDrawData();
    assert(data);
    assert(data->Valid);

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

    vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines.at(0));
    vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout,
        0,
        1, &mDescriptorSet,
        0, nullptr
    );
    // What should we do ?!
    if (resolution.width != 0 && resolution.height != 0)
    {
        assert(viewport.width > 0);
        assert(viewport.height > 0);
        vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
    }
    {// Constants
        DearImGuiConstants constants{};
        constants.scale    [0] =  2.0f / data->DisplaySize.x;
        constants.scale    [1] =  2.0f / data->DisplaySize.y;
        constants.translate[0] = -1.0f - data->DisplayPos.x * constants.scale[0];
        constants.translate[1] = -1.0f - data->DisplayPos.y * constants.scale[1];
        vkCmdPushConstants(commandbuffer, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);
    }
    if (data->TotalVtxCount > 0)
    {// Buffer Bindings
        constexpr VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandbuffer, kVertexInputBindingPosUVColor, 1, &mVertexBuffer.mBuffer, &offset);
        vkCmdBindIndexBuffer(commandbuffer, mIndexBuffer, offset, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
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

                    vkCmdSetScissor(commandbuffer, 0, 1, &scissors);
                    vkCmdDrawIndexed(commandbuffer, command.ElemCount, 1, command.IdxOffset + offset_index, command.VtxOffset + offset_vertex, 0);
                }
            }
            offset_index  += list->IdxBuffer.Size;
            offset_vertex += list->VtxBuffer.Size;
        }
    }
}

void VulkanPassUIOverlay::record_pass(VkCommandBuffer commandbuffer)
{
    record_frame(commandbuffer);
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
