#include "./vkpassscene.hpp"

#include "./vksize.hpp"
#include "./vkdebug.hpp"

#include "./vkmemory.hpp"
#include "./vkphysicaldevice.hpp"

#include "./vkengine.hpp"

#include "triangle-shader.hpp"

#include <vulkan/vulkan_core.h>

#include <cassert>

namespace
{
    constexpr std::uint32_t kStencilMask      = 0xFF;
    constexpr std::uint32_t kStencilReference = 0x01;

    using namespace blk;

    // TODO(andrea.machizaud) pre-allocate a reasonable amount for buffers
    constexpr std::size_t kInitialVertexBufferSize   = 1_MB;
    constexpr std::size_t kInitialIndexBufferSize    = 1_MB;
    constexpr std::size_t kInitialStagingBufferSize  = 1_MB;

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
        ~GraphicPipelineBuilderTriangle();
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
            .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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

    GraphicPipelineBuilderTriangle::~GraphicPipelineBuilderTriangle()
    {
        vkDestroyShaderModule(mDevice, mShader, nullptr);
    }
}

namespace blk::meshviewer
{

PassScene::PassScene(const blk::RenderPass& renderpass, std::uint32_t subpass, Arguments args)
    : Pass(renderpass, subpass)
    , mEngine(args.engine)
    , mDevice(renderpass.mDevice)
    , mResolution(args.resolution)
    , mUIMeshInformation(args.mesh_information)

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
}

PassScene::~PassScene()
{
    vkDestroySemaphore(mDevice, mStagingSemaphore, nullptr);
    vkDestroyFence(mDevice, mStagingFence, nullptr);

    vkDestroyCommandPool(mDevice, mGraphicsCommandPoolTransient, nullptr);

    vkDestroyPipeline(mDevice, mPipeline, nullptr);
    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
}

void PassScene::initialize_graphic_pipelines()
{
    assert(mPipeline == VK_NULL_HANDLE);

    VkPipeline pipelines[1];

    {
        std::array<VkGraphicsPipelineCreateInfo, 1> infos;
        GraphicPipelineBuilderTriangle triangle_builder(mDevice, mResolution, mPipelineLayout, mRenderPass, mSubpass, infos[0]);
        CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, infos.size(), infos.data(), nullptr, pipelines));
    }

    mPipeline = pipelines[0];
}

void PassScene::record_pass(VkCommandBuffer commandbuffer)
{
    vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
    vkCmdDraw(commandbuffer, 3, 1, 0, 0);
}

void PassScene::onResize(const VkExtent2D& resolution)
{
    mResolution = resolution;
    vkDestroyPipeline(mDevice, mPipeline, nullptr);
    mPipeline = VK_NULL_HANDLE;
    initialize_graphic_pipelines();
}

}
