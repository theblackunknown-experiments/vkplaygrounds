#include "./vkpassscene.hpp"

#include "./vkdebug.hpp"

#include "triangle-shader.hpp"

#include <vulkan/vulkan_core.h>

#include <cassert>

namespace
{
    constexpr std::uint32_t kStencilMask      = 0xFF;
    constexpr std::uint32_t kStencilReference = 0x01;
}

namespace blk::sample0
{

PassScene::PassScene(const blk::RenderPass& renderpass, std::uint32_t subpass, Arguments arguments)
    : Pass(renderpass, subpass)
    , mEngine(arguments.engine)
    , mDevice(renderpass.mDevice)
    , mResolution(arguments.resolution)
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
    initialize_graphic_pipelines();
}

PassScene::~PassScene()
{
    vkDestroyPipeline(mDevice, mPipeline, nullptr);
    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
}

void PassScene::initialize_graphic_pipelines()
{
    assert(mPipeline == VK_NULL_HANDLE);

    VkShaderModule shader = VK_NULL_HANDLE;

    {// Shader - UI
        constexpr VkShaderModuleCreateInfo info{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = kShaderTriangle.size() * sizeof(std::uint32_t),
            .pCode    = kShaderTriangle.data(),
        };
        CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &shader));
    }

    const std::array stages{
        VkPipelineShaderStageCreateInfo{
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_VERTEX_BIT,
            .module              = shader,
            .pName               = "triangle_main",
            .pSpecializationInfo = nullptr,
        },
        VkPipelineShaderStageCreateInfo{
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module              = shader,
            .pName               = "triangle_main",
            .pSpecializationInfo = nullptr,
        },
    };

    constexpr VkPipelineVertexInputStateCreateInfo vertexinput{
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                           = 0,
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = nullptr,
    };

    constexpr VkPipelineInputAssemblyStateCreateInfo assembly{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    const VkViewport fullviewport{
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = static_cast<float>(mResolution.width),
        .height   = static_cast<float>(mResolution.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    const VkRect2D fullscissors{
        .offset = VkOffset2D{
            .x = 0,
            .y = 0,
        },
        .extent = VkExtent2D{
            .width  = mResolution.width,
            .height = mResolution.height,
        }
    };

    const VkPipelineViewportStateCreateInfo viewport{
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .viewportCount = 1,
        .pViewports    = &fullviewport,
        .scissorCount  = 1,
        .pScissors     = &fullscissors,
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
    };

    constexpr std::array colorblendattachments{
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
    };

    static const/*expr*/ VkPipelineColorBlendStateCreateInfo colorblend{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .logicOpEnable           = VK_FALSE,
        .logicOp                 = VK_LOGIC_OP_COPY,
        .attachmentCount         = colorblendattachments.size(),
        .pAttachments            = colorblendattachments.data(),
        .blendConstants          = { 0.0f, 0.0f, 0.0f, 0.0f },
    };

    constexpr VkPipelineDynamicStateCreateInfo dynamics{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .dynamicStateCount       = 0,
    };

    const VkGraphicsPipelineCreateInfo info{
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
    };

    CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &info, nullptr, &mPipeline));

    vkDestroyShaderModule(mDevice, shader, nullptr);
}

void PassScene::record_pass(VkCommandBuffer commandbuffer)
{
    vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
    vkCmdDraw(commandbuffer, 3, 1, 0, 0);
}

}
