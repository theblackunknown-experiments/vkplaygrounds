#include "./vkpassscene.hpp"

#include "./vksize.hpp"
#include "./vkdebug.hpp"

#include "./vkmemory.hpp"
#include "./vkphysicaldevice.hpp"

#include "./vkengine.hpp"
#include "./vkmesh.hpp"

#include "obj-shader.hpp"
#include "triangle-shader.hpp"

#include <vulkan/vulkan_core.h>

#include <cassert>

#include <iterator>

#include <fstream>

#include <filesystem>

#include <obj2mesh.hpp>

namespace fs = std::filesystem;

namespace
{
    constexpr std::uint32_t kStencilMask      = 0xFF;
    constexpr std::uint32_t kStencilReference = 0x01;

    using namespace blk;

    // TODO(andrea.machizaud) pre-allocate a reasonable amount for buffers
    constexpr std::size_t kInitialVertexBufferSize   = 1_MB;
    constexpr std::size_t kInitialIndexBufferSize    = 1_MB;
    constexpr std::size_t kInitialStagingBufferSize  = 1_MB;

    constexpr VkFrontFace kTriangleFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    struct GraphicPipelineBuilderTriangle
    {
        VkDevice                                           mDevice;
        VkGraphicsPipelineCreateInfo&                      mInfo;

        VkViewport                                         mArea;
        VkRect2D                                           mScissors;
        std::array<VkPipelineColorBlendAttachmentState, 1> mBlendingAttachments;

        VkPipelineVertexInputStateCreateInfo               mVertexInput;
        VkPipelineInputAssemblyStateCreateInfo             mAssembly;
        VkPipelineViewportStateCreateInfo                  mViewport;
        VkPipelineRasterizationStateCreateInfo             mRasterization;
        VkPipelineMultisampleStateCreateInfo               mMultisample;
        VkPipelineDepthStencilStateCreateInfo              mDepthStencil;
        VkPipelineColorBlendStateCreateInfo                mBlending;
        VkPipelineDynamicStateCreateInfo                   mDynamics;

        VkShaderModule                                     mShader = VK_NULL_HANDLE;
        std::array<VkPipelineShaderStageCreateInfo, 2>     mStages;

        explicit GraphicPipelineBuilderTriangle(
            VkDevice                      vkdevice,
            VkExtent2D                    resolution,
            VkPipelineLayout              layout,
            VkRenderPass                  renderpass,
            std::uint32_t                 subpass,
            VkGraphicsPipelineCreateInfo& info,
            VkPipeline                    base_handle = VK_NULL_HANDLE,
            std::int32_t                  base_index = -1);
        ~GraphicPipelineBuilderTriangle()
        {
            vkDestroyShaderModule(mDevice, mShader, nullptr);
        }
    };

    GraphicPipelineBuilderTriangle::GraphicPipelineBuilderTriangle(
        VkDevice                      vkdevice,
        VkExtent2D                    resolution,
        VkPipelineLayout              layout,
        VkRenderPass                  renderpass,
        std::uint32_t                 subpass,
        VkGraphicsPipelineCreateInfo& info,
        VkPipeline                    base_handle,
        std::int32_t                  base_index)
        : mDevice(vkdevice)
        , mInfo(info)


        , mArea{
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<float>(resolution.width),
            .height   = static_cast<float>(resolution.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        }
        , mScissors{
            .offset = VkOffset2D{
                .x = 0,
                .y = 0,
            },
            .extent = VkExtent2D{
                .width  = resolution.width,
                .height = resolution.height,
            }
        }
        , mBlendingAttachments{
            VkPipelineColorBlendAttachmentState{
                .blendEnable         = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp        = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp        = VK_BLEND_OP_ADD,
                .colorWriteMask      =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT,
            }
        }

        , mVertexInput{
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext                           = nullptr,
            .flags                           = 0,
            .vertexBindingDescriptionCount   = 0,
            .pVertexBindingDescriptions      = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions    = nullptr,
        }
        , mAssembly{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        }
        , mViewport{
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = 0,
            .viewportCount = 1,
            .pViewports    = &mArea,
            .scissorCount  = 1,
            .pScissors     = &mScissors,
        }
        , mRasterization{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .cullMode                = VK_CULL_MODE_NONE,
            .frontFace               = kTriangleFrontFace,
            .depthBiasEnable         = VK_FALSE,
            .lineWidth               = 1.0f,
        }
        , mMultisample{
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = 0.0f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE,
        }
        , mDepthStencil{
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .depthTestEnable       = VK_FALSE,
            .depthWriteEnable      = VK_FALSE,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable     = VK_TRUE,
            .front                 = VkStencilOpState{
                .failOp      = VK_STENCIL_OP_KEEP,
                .passOp      = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp   = VK_COMPARE_OP_NOT_EQUAL,
                .compareMask = kStencilMask,
                .writeMask   = kStencilMask,
                .reference   = kStencilReference,
            },
            .back                  = VkStencilOpState{
                .failOp      = VK_STENCIL_OP_KEEP,
                .passOp      = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp   = VK_COMPARE_OP_NOT_EQUAL,
                .compareMask = kStencilMask,
                .writeMask   = kStencilMask,
                .reference   = kStencilReference,
            },
        }
        , mBlending{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .logicOpEnable           = VK_FALSE,
            .logicOp                 = VK_LOGIC_OP_COPY,
            .attachmentCount         = static_cast<std::uint32_t>(mBlendingAttachments.size()),
            .pAttachments            = mBlendingAttachments.data(),
            .blendConstants          = { 0.0f, 0.0f, 0.0f, 0.0f },
        }
        , mDynamics{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .dynamicStateCount       = 0,
        }
    {
        {// Shader
            constexpr VkShaderModuleCreateInfo info{
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext    = nullptr,
                .flags    = 0,
                .codeSize = kShaderTriangle.size() * sizeof(std::uint32_t),
                .pCode    = kShaderTriangle.data(),
            };
            CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &mShader));
        }

        VkPipelineShaderStageCreateInfo& stage_vertex = mStages.at(0);
        stage_vertex.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_vertex.pNext               = nullptr;
        stage_vertex.flags               = 0;
        stage_vertex.stage               = VK_SHADER_STAGE_VERTEX_BIT;
        stage_vertex.module              = mShader;
        stage_vertex.pName               = "triangle_main";
        stage_vertex.pSpecializationInfo = nullptr;

        VkPipelineShaderStageCreateInfo& stage_fragment = mStages.at(1);
        stage_fragment.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_fragment.pNext               = nullptr;
        stage_fragment.flags               = 0;
        stage_fragment.stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage_fragment.module              = mShader;
        stage_fragment.pName               = "triangle_main";
        stage_fragment.pSpecializationInfo = nullptr;

        mInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        mInfo.pNext               = nullptr;
        mInfo.flags               = 0;
        mInfo.stageCount          = mStages.size();
        mInfo.pStages             = mStages.data();
        mInfo.pVertexInputState   = &mVertexInput;
        mInfo.pInputAssemblyState = &mAssembly;
        mInfo.pTessellationState  = nullptr;
        mInfo.pViewportState      = &mViewport;
        mInfo.pRasterizationState = &mRasterization;
        mInfo.pMultisampleState   = &mMultisample;
        mInfo.pDepthStencilState  = &mDepthStencil;
        mInfo.pColorBlendState    = &mBlending;
        mInfo.pDynamicState       = &mDynamics;
        mInfo.layout              = layout;
        mInfo.renderPass          = renderpass;
        mInfo.subpass             = subpass;
        mInfo.basePipelineHandle  = base_handle;
        mInfo.basePipelineIndex   = base_index;
    }

    struct GraphicPipelineBuilderOBJ
    {
        VkDevice                                           mDevice;
        VkGraphicsPipelineCreateInfo&                      mInfo;

        std::array<VkVertexInputBindingDescription, 1>     mVertexBindings;
        std::array<VkVertexInputAttributeDescription, 1>   mVertexAttributes;
        VkViewport                                         mArea;
        VkRect2D                                           mScissors;
        std::array<VkPipelineColorBlendAttachmentState, 1> mBlendingAttachments;

        VkPipelineVertexInputStateCreateInfo               mVertexInput;
        VkPipelineInputAssemblyStateCreateInfo             mAssembly;
        VkPipelineViewportStateCreateInfo                  mViewport;
        VkPipelineRasterizationStateCreateInfo             mRasterization;
        VkPipelineMultisampleStateCreateInfo               mMultisample;
        VkPipelineDepthStencilStateCreateInfo              mDepthStencil;
        VkPipelineColorBlendStateCreateInfo                mBlending;
        VkPipelineDynamicStateCreateInfo                   mDynamics;

        VkShaderModule                                     mShader = VK_NULL_HANDLE;
        std::array<VkPipelineShaderStageCreateInfo, 2>     mStages;

        explicit GraphicPipelineBuilderOBJ(
            VkDevice                      vkdevice,
            VkExtent2D                    resolution,
            VkPipelineLayout              layout,
            VkRenderPass                  renderpass,
            std::uint32_t                 subpass,
            VkGraphicsPipelineCreateInfo& info,
            VkPipeline                    base_handle = VK_NULL_HANDLE,
            std::int32_t                  base_index = -1);
        ~GraphicPipelineBuilderOBJ()
        {
            vkDestroyShaderModule(mDevice, mShader, nullptr);
        }
    };

    constexpr std::uint32_t kVertexInputBindingPositionOBJ = 0;

    constexpr std::uint32_t kVertexAttributeLocationPositionOBJ = 0;

    GraphicPipelineBuilderOBJ::GraphicPipelineBuilderOBJ(
        VkDevice                      vkdevice,
        VkExtent2D                    resolution,
        VkPipelineLayout              layout,
        VkRenderPass                  renderpass,
        std::uint32_t                 subpass,
        VkGraphicsPipelineCreateInfo& info,
        VkPipeline                    base_handle,
        std::int32_t                  base_index)
        : mDevice(vkdevice)
        , mInfo(info)


        , mVertexBindings{
            VkVertexInputBindingDescription{
                .binding   = kVertexInputBindingPositionOBJ,
                .stride    = sizeof(blk::meshes::obj::vertex_t),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
        }
        , mVertexAttributes{
            VkVertexInputAttributeDescription{
                .location  = kVertexAttributeLocationPositionOBJ,
                .binding   = kVertexInputBindingPositionOBJ,
                .format    = VK_FORMAT_R32G32B32_SFLOAT,
                .offset    = 0,
            }
        }
        , mArea{
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<float>(resolution.width),
            .height   = static_cast<float>(resolution.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        }
        , mScissors{
            .offset = VkOffset2D{
                .x = 0,
                .y = 0,
            },
            .extent = VkExtent2D{
                .width  = resolution.width,
                .height = resolution.height,
            }
        }
        , mBlendingAttachments{
            VkPipelineColorBlendAttachmentState{
                .blendEnable         = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp        = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp        = VK_BLEND_OP_ADD,
                .colorWriteMask      =
                    VK_COLOR_COMPONENT_R_BIT |
                    VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT |
                    VK_COLOR_COMPONENT_A_BIT,
            }
        }

        , mVertexInput{
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext                           = nullptr,
            .flags                           = 0,
            .vertexBindingDescriptionCount   = static_cast<std::uint32_t>(mVertexBindings.size()),
            .pVertexBindingDescriptions      = mVertexBindings.data(),
            .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(mVertexAttributes.size()),
            .pVertexAttributeDescriptions    = mVertexAttributes.data(),
        }
        , mAssembly{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        }
        , mViewport{
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = 0,
            .viewportCount = 1,
            .pViewports    = &mArea,
            .scissorCount  = 1,
            .pScissors     = &mScissors,
        }
        , mRasterization{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .cullMode                = VK_CULL_MODE_NONE,
            .frontFace               = kTriangleFrontFace,
            .depthBiasEnable         = VK_FALSE,
            .lineWidth               = 1.0f,
        }
        , mMultisample{
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = 0.0f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE,
        }
        , mDepthStencil{
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .depthTestEnable       = VK_FALSE,
            .depthWriteEnable      = VK_FALSE,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable     = VK_TRUE,
            .front                 = VkStencilOpState{
                .failOp      = VK_STENCIL_OP_KEEP,
                .passOp      = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp   = VK_COMPARE_OP_NOT_EQUAL,
                .compareMask = kStencilMask,
                .writeMask   = kStencilMask,
                .reference   = kStencilReference,
            },
            .back                  = VkStencilOpState{
                .failOp      = VK_STENCIL_OP_KEEP,
                .passOp      = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp   = VK_COMPARE_OP_NOT_EQUAL,
                .compareMask = kStencilMask,
                .writeMask   = kStencilMask,
                .reference   = kStencilReference,
            },
        }
        , mBlending{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .logicOpEnable           = VK_FALSE,
            .logicOp                 = VK_LOGIC_OP_COPY,
            .attachmentCount         = static_cast<std::uint32_t>(mBlendingAttachments.size()),
            .pAttachments            = mBlendingAttachments.data(),
            .blendConstants          = { 0.0f, 0.0f, 0.0f, 0.0f },
        }
        , mDynamics{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .dynamicStateCount       = 0,
        }
    {
        {// Shader
            constexpr VkShaderModuleCreateInfo info{
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext    = nullptr,
                .flags    = 0,
                .codeSize = kShaderOBJ.size() * sizeof(std::uint32_t),
                .pCode    = kShaderOBJ.data(),
            };
            CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &mShader));
        }

        VkPipelineShaderStageCreateInfo& stage_vertex = mStages.at(0);
        stage_vertex.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_vertex.pNext               = nullptr;
        stage_vertex.flags               = 0;
        stage_vertex.stage               = VK_SHADER_STAGE_VERTEX_BIT;
        stage_vertex.module              = mShader;
        stage_vertex.pName               = "obj_main";
        stage_vertex.pSpecializationInfo = nullptr;

        VkPipelineShaderStageCreateInfo& stage_fragment = mStages.at(1);
        stage_fragment.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_fragment.pNext               = nullptr;
        stage_fragment.flags               = 0;
        stage_fragment.stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage_fragment.module              = mShader;
        stage_fragment.pName               = "obj_main";
        stage_fragment.pSpecializationInfo = nullptr;

        mInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        mInfo.pNext               = nullptr;
        mInfo.flags               = 0;
        mInfo.stageCount          = mStages.size();
        mInfo.pStages             = mStages.data();
        mInfo.pVertexInputState   = &mVertexInput;
        mInfo.pInputAssemblyState = &mAssembly;
        mInfo.pTessellationState  = nullptr;
        mInfo.pViewportState      = &mViewport;
        mInfo.pRasterizationState = &mRasterization;
        mInfo.pMultisampleState   = &mMultisample;
        mInfo.pDepthStencilState  = &mDepthStencil;
        mInfo.pColorBlendState    = &mBlending;
        mInfo.pDynamicState       = &mDynamics;
        mInfo.layout              = layout;
        mInfo.renderPass          = renderpass;
        mInfo.subpass             = subpass;
        mInfo.basePipelineHandle  = base_handle;
        mInfo.basePipelineIndex   = base_index;
    }
}

namespace blk::meshviewer
{

PassScene::PassScene(const blk::RenderPass& renderpass, std::uint32_t subpass, Arguments args)
    : Pass(renderpass, subpass)
    , mEngine(args.engine)
    , mDevice(renderpass.mDevice)
    , mResolution(args.resolution)

    , previous_selected_mesh_index(args.shared_data.mSelectedMeshIndex)
    , mSharedData(args.shared_data)

    , mVertexBuffer(kInitialVertexBufferSize  , VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
    , mIndexBuffer(kInitialIndexBufferSize    , VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)

    , mStagingBuffer(kInitialStagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
{
    {// Pipeline Layouts
        const VkPipelineLayoutCreateInfo info{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = 0,
            .pushConstantRangeCount = 0,
        };
        CHECK(vkCreatePipelineLayout(mDevice, &info, nullptr, &mPipelineLayout));
    }
    {// Buffers
        mVertexBuffer.create(mDevice);
        mIndexBuffer.create(mDevice);
        mStagingBuffer.create(mDevice);
    }
    {// Memories
        auto vkphysicaldevice = *(mDevice.mPhysicalDevice);

        auto memory_type_vertex  = vkphysicaldevice.mMemories.find_compatible(mVertexBuffer, 0);
        auto memory_type_index   = vkphysicaldevice.mMemories.find_compatible(mIndexBuffer, 0);
        auto memory_type_staging = vkphysicaldevice.mMemories.find_compatible(mStagingBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        assert(memory_type_vertex);
        assert(memory_type_index);
        assert(memory_type_staging);

        assert(memory_type_index == memory_type_vertex);

        mGeometryMemory = std::make_unique<blk::Memory>(*memory_type_index, mVertexBuffer.mRequirements.size + mIndexBuffer.mRequirements.size);
        mGeometryMemory->allocate(mDevice);
        mGeometryMemory->bind({ &mVertexBuffer, &mIndexBuffer });

        mStagingMemory = std::make_unique<blk::Memory>(*memory_type_staging, mStagingBuffer.mRequirements.size);
        mStagingMemory->allocate(mDevice);
        mStagingMemory->bind(mStagingBuffer);
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
    {// Queues / Command Pool
        assert(!mEngine.mGraphicsQueues.empty());

        mGraphicsQueue = mEngine.mGraphicsQueues.at(0);

        {// Command Pools - One Off
            const VkCommandPoolCreateInfo info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                .queueFamilyIndex = mGraphicsQueue->mFamily.mIndex,
            };
            CHECK(vkCreateCommandPool(mDevice, &info, nullptr, &mGraphicsCommandPoolTransient));
        }
    }
    {// Command Buffers
        const VkCommandBufferAllocateInfo info{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = mGraphicsCommandPoolTransient,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        CHECK(vkAllocateCommandBuffers(mDevice, &info, &mStagingCommandBuffer));
    }
    {// Fences
        const VkFenceCreateInfo info{
            .sType              = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext              = nullptr,
            .flags              = 0,
        };
        CHECK(vkCreateFence(mDevice, &info, nullptr, &mStagingFence));
    }
    {// Semaphore
        const VkSemaphoreTypeCreateInfo info_type{
            .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext         = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
            .initialValue  = 0,
        };
        const VkSemaphoreCreateInfo info_semaphore{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &info_type,
            .flags = 0,
        };
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mStagingSemaphore));
    }
    initialize_graphic_pipelines();
    {// Mesh
        upload_mesh_from_path(mStagingBuffer, mSharedData.mMeshPaths[mSharedData.mSelectedMeshIndex]);
    }
}

PassScene::~PassScene()
{
    vkDestroySemaphore(mDevice, mStagingSemaphore, nullptr);
    vkDestroyFence(mDevice, mStagingFence, nullptr);

    vkDestroyCommandPool(mDevice, mGraphicsCommandPoolTransient, nullptr);

    vkDestroyPipeline(mDevice, mPipelineDefault, nullptr);
    vkDestroyPipeline(mDevice, mPipelineOBJ, nullptr);
    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
}

void PassScene::initialize_graphic_pipelines()
{
    assert(mPipelineDefault == VK_NULL_HANDLE);
    assert(mPipelineOBJ == VK_NULL_HANDLE);

    VkPipeline pipelines[2];

    {
        std::array<VkGraphicsPipelineCreateInfo, 2> infos;
        GraphicPipelineBuilderTriangle triangle_builder(mDevice, mResolution, mPipelineLayout, mRenderPass, mSubpass, infos[0]);
        GraphicPipelineBuilderOBJ      obj_builder(mDevice, mResolution, mPipelineLayout, mRenderPass, mSubpass, infos[1]);
        CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, infos.size(), infos.data(), nullptr, pipelines));
    }

    mPipelineDefault = pipelines[0];
    mPipelineOBJ = pipelines[1];
}

void PassScene::upload_mesh_from_path(blk::Buffer& staging_buffer, const fs::path& mesh_path)
{
    assert(mesh_path.has_extension());
    assert(mesh_path.extension().string() == ".obj");
    assert(fs::exists(mesh_path));

    std::ifstream stream(mesh_path, std::ios::in);
    assert(stream.is_open());

    std::istreambuf_iterator<char> streambegin(stream), streamend;
    mParsedOBJ = blk::meshes::obj::parse_obj_stream(mesh_path.parent_path(), streambegin, streamend);

    stream.close();

    mMeshOBJ = blk::meshes::obj::wrap_as_buffers(mParsedOBJ);

    assert(mStagingBuffer.mMemory != nullptr);

    {// Command Buffer - Allocate
        const VkCommandBufferAllocateInfo info{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = mGraphicsCommandPoolTransient,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        CHECK(vkAllocateCommandBuffers(mDevice, &info, &mStagingCommandBuffer));
    }

    {// Vertices
        const std::size_t vertices_bytes_size = sizeof(blk::meshes::obj::vertex_t) * mParsedOBJ.vertices.size();

        assert(vertices_bytes_size <= mStagingBuffer.mMemory->mInfo.allocationSize);
        {// Upload vertices -> Staging
            void* mapped = nullptr;
            CHECK(vkMapMemory(
                mDevice,
                *(mStagingBuffer.mMemory),
                mStagingBuffer.mOffset,
                mStagingBuffer.mRequirements.size,
                0,
                &mapped
            ));

            std::memcpy(mapped, mParsedOBJ.vertices.data(), vertices_bytes_size);

            vkUnmapMemory(mDevice, *staging_buffer.mMemory);
            mStagingBuffer.mOccupied = vertices_bytes_size;
        }

        {// Command Buffer - Recording
            constexpr VkCommandBufferBeginInfo info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext            = nullptr,
                .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                .pInheritanceInfo = nullptr,
            };
            CHECK(vkBeginCommandBuffer(mStagingCommandBuffer, &info));

            {// Copy Staging Buffer -> Vertex Buffer
                const VkBufferCopy region{
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size      = vertices_bytes_size,
                };
                vkCmdCopyBuffer(
                    mStagingCommandBuffer,
                    mStagingBuffer, mVertexBuffer,
                    1, &region
                );
            }
            CHECK(vkEndCommandBuffer(mStagingCommandBuffer));
        }
        {// Submit
            const std::array infos{
                VkSubmitInfo{
                    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .pNext                = nullptr,
                    .waitSemaphoreCount   = 0,
                    .pWaitSemaphores      = nullptr,
                    .pWaitDstStageMask    = 0,
                    .commandBufferCount   = 1,
                    .pCommandBuffers      = &mStagingCommandBuffer,
                    .signalSemaphoreCount = 0,
                    .pSignalSemaphores    = nullptr,
                }
            };
            CHECK(vkQueueSubmit(*mGraphicsQueue, infos.size(), infos.data(), mStagingFence));
            CHECK(vkWaitForFences(mDevice, 1, &mStagingFence, VK_TRUE, UINT64_MAX));
        }
        mVertexBuffer.mOccupied = vertices_bytes_size;
    }
    {// Reset
        CHECK(vkResetFences(mDevice, 1, &mStagingFence));
        CHECK(vkResetCommandPool(mDevice, mGraphicsCommandPoolTransient, 0));
    }
    {// Indices

        std::size_t indices_count = 0;
        for (auto&& group : mParsedOBJ.groups)
        {
            // Each face yields 2 triangles
            indices_count += group.faces.size() * 2 * 3;
        }
        std::vector<std::uint16_t> indices(indices_count);

        const std::size_t indices_bytes_size = indices_count * sizeof(std::uint16_t);
        assert(indices_bytes_size <= mStagingBuffer.mMemory->mInfo.allocationSize);

        {// Compute Index Buffer : Triangulate incoming faces
            for (auto&& group : mParsedOBJ.groups)
            {
                for (auto&& face : group.faces)
                {
                    auto is_quad = face.points[0] != static_cast<std::size_t>(~0)
                        && face.points[1] != static_cast<std::size_t>(~0)
                        && face.points[2] != static_cast<std::size_t>(~0)
                        && face.points[3] != static_cast<std::size_t>(~0);
                    auto is_triangle = face.points[0] != static_cast<std::size_t>(~0)
                        && face.points[1] != static_cast<std::size_t>(~0)
                        && face.points[2] != static_cast<std::size_t>(~0)
                        && face.points[3] == static_cast<std::size_t>(~0);

                    assert(is_quad);

                    auto& p0 = mParsedOBJ.points.at(face.points[0]);
                    auto& p1 = mParsedOBJ.points.at(face.points[1]);
                    auto& p2 = mParsedOBJ.points.at(face.points[2]);
                    auto& p3 = mParsedOBJ.points.at(face.points[3]);

                    auto v0 = p0.vertex;
                    auto v1 = p1.vertex;
                    auto v2 = p2.vertex;
                    auto v3 = p3.vertex;

                    assert(v0 <= std::numeric_limits<std::uint16_t>::max());
                    assert(v1 <= std::numeric_limits<std::uint16_t>::max());
                    assert(v2 <= std::numeric_limits<std::uint16_t>::max());
                    assert(v3 <= std::numeric_limits<std::uint16_t>::max());

                    /*
                        p2 ---- p3
                         | \     |
                         |  \    |
                         |   \   |
                         |    \  |
                        p1 ---- p0
                    */

                    if constexpr (kTriangleFrontFace == VK_FRONT_FACE_COUNTER_CLOCKWISE)
                    {
                        indices.push_back( static_cast<std::uint16_t>(v0) );
                        indices.push_back( static_cast<std::uint16_t>(v2) );
                        indices.push_back( static_cast<std::uint16_t>(v1) );

                        indices.push_back( static_cast<std::uint16_t>(v0) );
                        indices.push_back( static_cast<std::uint16_t>(v3) );
                        indices.push_back( static_cast<std::uint16_t>(v2) );
                    }
                    else
                    {
                        indices.push_back( static_cast<std::uint16_t>(v0) );
                        indices.push_back( static_cast<std::uint16_t>(v1) );
                        indices.push_back( static_cast<std::uint16_t>(v2) );

                        indices.push_back( static_cast<std::uint16_t>(v0) );
                        indices.push_back( static_cast<std::uint16_t>(v2) );
                        indices.push_back( static_cast<std::uint16_t>(v3) );
                    }
                }
            }
        }

        {// Upload vertices -> Staging
            void* mapped = nullptr;
            CHECK(vkMapMemory(
                mDevice,
                *(mStagingBuffer.mMemory),
                mStagingBuffer.mOffset,
                mStagingBuffer.mRequirements.size,
                0,
                &mapped
            ));

            std::memcpy(mapped, mParsedOBJ.vertices.data(), indices_bytes_size);

            vkUnmapMemory(mDevice, *staging_buffer.mMemory);
            mStagingBuffer.mOccupied = indices_bytes_size;
        }
        {// Command Buffer - Recording
            constexpr VkCommandBufferBeginInfo info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .pInheritanceInfo = nullptr,
            };
            CHECK(vkBeginCommandBuffer(mStagingCommandBuffer, &info));

            {// Copy Staging Buffer -> Vertex Buffer
                const VkBufferCopy region{
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size      = indices_bytes_size,
                };
                vkCmdCopyBuffer(
                    mStagingCommandBuffer,
                    mStagingBuffer, mIndexBuffer,
                    1, &region
                );
            }
            CHECK(vkEndCommandBuffer(mStagingCommandBuffer));
        }
        {// Submit
            const std::array infos{
                VkSubmitInfo{
                    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .pNext                = nullptr,
                    .waitSemaphoreCount   = 0,
                    .pWaitSemaphores      = nullptr,
                    .pWaitDstStageMask    = 0,
                    .commandBufferCount   = 1,
                    .pCommandBuffers      = &mStagingCommandBuffer,
                    .signalSemaphoreCount = 0,
                    .pSignalSemaphores    = nullptr,
                }
            };
            CHECK(vkQueueSubmit(*mGraphicsQueue, infos.size(), infos.data(), mStagingFence));
            CHECK(vkWaitForFences(mDevice, 1, &mStagingFence, VK_TRUE, UINT64_MAX));
        }
        mIndexBuffer.mOccupied = indices_bytes_size;
    }
    {// Reset
        CHECK(vkResetFences(mDevice, 1, &mStagingFence));
        CHECK(vkResetCommandPool(mDevice, mGraphicsCommandPoolTransient, 0));
    }
    {// Command Buffer - Deallocate
        vkFreeCommandBuffers(mDevice, mGraphicsCommandPoolTransient, 1, &mStagingCommandBuffer);
    }
}

void PassScene::record_pass(VkCommandBuffer commandbuffer)
{
    if (mSharedData.mVisualizeMesh)
    {
        vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineOBJ);

        constexpr VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandbuffer, kVertexInputBindingPositionOBJ, 1, &mVertexBuffer.mBuffer, &offset);
        vkCmdBindIndexBuffer(commandbuffer, mIndexBuffer, offset, VK_INDEX_TYPE_UINT16);

        const std::uint32_t indices_count = mIndexBuffer.mOccupied / sizeof(std::uint16_t);
        vkCmdDrawIndexed(commandbuffer, indices_count, 1, 0, 0, 0);
    }
    else
    {
        vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineDefault);
        vkCmdDraw(commandbuffer, 3, 1, 0, 0);
    }
}

void PassScene::onResize(const VkExtent2D& resolution)
{
    mResolution = resolution;
    vkDestroyPipeline(mDevice, mPipelineDefault, nullptr);
    vkDestroyPipeline(mDevice, mPipelineOBJ, nullptr);
    mPipelineDefault = VK_NULL_HANDLE;
    mPipelineOBJ = VK_NULL_HANDLE;
    initialize_graphic_pipelines();
}

}
