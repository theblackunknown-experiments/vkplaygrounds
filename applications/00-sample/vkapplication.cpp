#include "./vkapplication.hpp"

#include "./vkengine.hpp"
#include "./vkdevice.hpp"
#include "./vkphysicaldevice.hpp"

#include "./vkmemory.hpp"

#include <vkpresentation.hpp>

#include <triangle-shader.hpp>
#include <default-triangle-shader.hpp>
#include <mesh-shader.hpp>

#include <vulkan/vulkan_core.h>

#include <span>
#include <array>
#include <ranges>
#include <vector>

#include <range/v3/view/zip.hpp>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

namespace
{
// NVIDIA - https://developer.nvidia.com/blog/vulkan-dos-donts/
//  > Prefer using 24 bit depth formats for optimal performance
//  > Prefer using packed depth/stencil formats.
//      This is a common cause for notable performance differences between an OpenGL and Vulkan implementation.
constexpr std::array kPreferredDepthFormats{
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D16_UNORM_S8_UINT,
	VK_FORMAT_D32_SFLOAT_S8_UINT,
	VK_FORMAT_D16_UNORM,
	VK_FORMAT_D32_SFLOAT,
};

struct GraphicPipelineBuilder
{
	VkDevice mDevice;
	VkGraphicsPipelineCreateInfo& mInfo;

	VkViewport mArea;
	VkRect2D mScissors;
	std::array<VkPipelineColorBlendAttachmentState, 1> mBlendingAttachments;

	VkPipelineVertexInputStateCreateInfo mVertexInput;
	VkPipelineInputAssemblyStateCreateInfo mAssembly;
	VkPipelineViewportStateCreateInfo mViewport;
	VkPipelineRasterizationStateCreateInfo mRasterization;
	VkPipelineMultisampleStateCreateInfo mMultisample;
	VkPipelineColorBlendStateCreateInfo mBlending;
	VkPipelineDynamicStateCreateInfo mDynamics;

	VkShaderModule mShader = VK_NULL_HANDLE;
	std::array<VkPipelineShaderStageCreateInfo, 2> mStages;

	explicit GraphicPipelineBuilder(
		VkDevice vkdevice,
		VkExtent2D resolution,
		VkPipelineLayout layout,
		VkRenderPass renderpass,
		std::uint32_t subpass,
		VkGraphicsPipelineCreateInfo& info,
        const std::vector<std::uint32_t>& shader_code,
        const char* entry_point,
		VkPipeline base_handle = VK_NULL_HANDLE,
		std::int32_t base_index = -1)
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
            .extent = resolution
        }
        , mBlendingAttachments{
            VkPipelineColorBlendAttachmentState{
                .blendEnable         = VK_FALSE,
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
            .frontFace               = VK_FRONT_FACE_CLOCKWISE,
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
		{ // Shader
			const VkShaderModuleCreateInfo info{
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.codeSize = shader_code.size() * sizeof(std::uint32_t),
				.pCode = shader_code.data(),
			};
			CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &mShader));
		}

		VkPipelineShaderStageCreateInfo& stage_vertex = mStages.at(0);
		stage_vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_vertex.pNext = nullptr;
		stage_vertex.flags = 0;
		stage_vertex.stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage_vertex.module = mShader;
		stage_vertex.pName = entry_point;
		stage_vertex.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo& stage_fragment = mStages.at(1);
		stage_fragment.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_fragment.pNext = nullptr;
		stage_fragment.flags = 0;
		stage_fragment.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stage_fragment.module = mShader;
		stage_fragment.pName = entry_point;
		stage_fragment.pSpecializationInfo = nullptr;

		mInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		mInfo.pNext = nullptr;
		mInfo.flags = 0;
		mInfo.stageCount = mStages.size();
		mInfo.pStages = mStages.data();
		mInfo.pVertexInputState = &mVertexInput;
		mInfo.pInputAssemblyState = &mAssembly;
		mInfo.pTessellationState = nullptr;
		mInfo.pViewportState = &mViewport;
		mInfo.pRasterizationState = &mRasterization;
		mInfo.pMultisampleState = &mMultisample;
		mInfo.pDepthStencilState = nullptr;
		mInfo.pColorBlendState = &mBlending;
		mInfo.pDynamicState = &mDynamics;
		mInfo.layout = layout;
		mInfo.renderPass = renderpass;
		mInfo.subpass = subpass;
		mInfo.basePipelineHandle = base_handle;
		mInfo.basePipelineIndex = base_index;
	}
	~GraphicPipelineBuilder() { vkDestroyShaderModule(mDevice, mShader, nullptr); }
};

struct GraphicPipelineBuilderMesh
{
	VkDevice mDevice;
	VkGraphicsPipelineCreateInfo& mInfo;

	VkViewport mArea;
	VkRect2D mScissors;
	std::array<VkPipelineColorBlendAttachmentState, 1> mBlendingAttachments;
	blk::VertexInputDescription mVertexInputDescription = blk::Vertex::get_description();

	VkPipelineVertexInputStateCreateInfo mVertexInput;
	VkPipelineInputAssemblyStateCreateInfo mAssembly;
	VkPipelineViewportStateCreateInfo mViewport;
	VkPipelineRasterizationStateCreateInfo mRasterization;
	VkPipelineMultisampleStateCreateInfo mMultisample;
	VkPipelineColorBlendStateCreateInfo mBlending;
	VkPipelineDynamicStateCreateInfo mDynamics;

	VkShaderModule mShader = VK_NULL_HANDLE;
	std::array<VkPipelineShaderStageCreateInfo, 2> mStages;

	explicit GraphicPipelineBuilderMesh(
        VkDevice vkdevice,
        VkExtent2D resolution,
        VkPipelineLayout layout,
        VkRenderPass renderpass,
        std::uint32_t subpass,
        VkGraphicsPipelineCreateInfo& info,
        const std::vector<std::uint32_t>& shader_code,
        const char* entry_point,
        VkPipeline base_handle = VK_NULL_HANDLE,
        std::int32_t base_index = -1)
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
            .extent = resolution
        }
        , mBlendingAttachments{
            VkPipelineColorBlendAttachmentState{
                .blendEnable         = VK_FALSE,
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
            .vertexBindingDescriptionCount   = static_cast<std::uint32_t>(mVertexInputDescription.bindings.size()),
            .pVertexBindingDescriptions      = mVertexInputDescription.bindings.data(),
            .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(mVertexInputDescription.attributes.size()),
            .pVertexAttributeDescriptions    = mVertexInputDescription.attributes.data(),
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
            .frontFace               = VK_FRONT_FACE_CLOCKWISE,
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
		{ // Shader
			const VkShaderModuleCreateInfo info{
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.codeSize = shader_code.size() * sizeof(std::uint32_t),
				.pCode = shader_code.data(),
			};
			CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &mShader));
		}

		VkPipelineShaderStageCreateInfo& stage_vertex = mStages.at(0);
		stage_vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_vertex.pNext = nullptr;
		stage_vertex.flags = 0;
		stage_vertex.stage = VK_SHADER_STAGE_VERTEX_BIT;
		stage_vertex.module = mShader;
		stage_vertex.pName = entry_point;
		stage_vertex.pSpecializationInfo = nullptr;

		VkPipelineShaderStageCreateInfo& stage_fragment = mStages.at(1);
		stage_fragment.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage_fragment.pNext = nullptr;
		stage_fragment.flags = 0;
		stage_fragment.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stage_fragment.module = mShader;
		stage_fragment.pName = entry_point;
		stage_fragment.pSpecializationInfo = nullptr;

		mInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		mInfo.pNext = nullptr;
		mInfo.flags = 0;
		mInfo.stageCount = mStages.size();
		mInfo.pStages = mStages.data();
		mInfo.pVertexInputState = &mVertexInput;
		mInfo.pInputAssemblyState = &mAssembly;
		mInfo.pTessellationState = nullptr;
		mInfo.pViewportState = &mViewport;
		mInfo.pRasterizationState = &mRasterization;
		mInfo.pMultisampleState = &mMultisample;
		mInfo.pDepthStencilState = nullptr;
		mInfo.pColorBlendState = &mBlending;
		mInfo.pDynamicState = &mDynamics;
		mInfo.layout = layout;
		mInfo.renderPass = renderpass;
		mInfo.subpass = subpass;
		mInfo.basePipelineHandle = base_handle;
		mInfo.basePipelineIndex = base_index;
	}
	~GraphicPipelineBuilderMesh() { vkDestroyShaderModule(mDevice, mShader, nullptr); }
};

struct MeshPushConstants
{
	glm::vec4 data;
	glm::mat4 render_matrix;
};

} // namespace

namespace blk::sample00
{
Application::RenderPass::RenderPass(Engine& vkengine, VkFormat formatColor, VkFormat formatDepth)
	: ::blk::RenderPass(vkengine.mDevice)
{
	constexpr std::uint32_t kSubpass = 0;
	constexpr std::uint32_t kSubpassUI = 0;
	constexpr std::uint32_t kSubpassScene = 1;

	constexpr std::uint32_t kAttachmentColor = 0;
	constexpr std::uint32_t kAttachmentDepth = 1;

	const std::array attachments{
		VkAttachmentDescription{
			.flags = 0,
			.format = formatColor,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		},
	};

	constexpr VkAttachmentReference write_color_reference{
		.attachment = kAttachmentColor,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	const std::array subpasses{
		VkSubpassDescription{
			.flags = 0,
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &write_color_reference,
			.pResolveAttachments = nullptr,
			.pDepthStencilAttachment = nullptr,
			.preserveAttachmentCount = 0,
			.pPreserveAttachments = nullptr,
		},
	};
	const VkRenderPassCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = attachments.size(),
		.pAttachments = attachments.data(),
		.subpassCount = subpasses.size(),
		.pSubpasses = subpasses.data(),
		.dependencyCount = 0,
		.pDependencies = nullptr,
	};
	CHECK(create(info));
}

Application::Application(blk::Engine& vkengine, blk::Presentation& vkpresentation)
	: mEngine(vkengine)
	, mPresentation(vkpresentation)
	, mDevice(vkengine.mDevice)

	, mColorFormat(mPresentation.mColorFormat)
	, mDepthFormat([&vkengine] {
		auto finder = std::ranges::find_if(kPreferredDepthFormats, [&vkengine](const VkFormat& f) {
			VkFormatProperties properties;
			vkGetPhysicalDeviceFormatProperties(*vkengine.mDevice.mPhysicalDevice, f, &properties);
			return properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
		});

		assert(finder != std::end(kPreferredDepthFormats));
		return *finder;
	}())

	, mResolution(mPresentation.mResolution)

	, mRenderPass(vkengine, mColorFormat, mDepthFormat)

	// , mMultipass(
	// 	  mRenderPass,
	// 	  PassUIOverlay::Arguments{vkengine, resolution, mSharedData},
	// 	  PassScene::Arguments{vkengine, resolution, mSharedData})
	// , mPassUIOverlay(subpass<0>(mMultipass))
	// , mPassScene(subpass<1>(mMultipass))

	, mDepthImage(
		  VkExtent3D{.width = mResolution.width, .height = mResolution.height, .depth = 1},
		  VK_IMAGE_TYPE_2D,
		  mDepthFormat,
		  VK_SAMPLE_COUNT_1_BIT,
		  VK_IMAGE_TILING_OPTIMAL,
		  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		  VK_IMAGE_LAYOUT_UNDEFINED)

	, mRenderSemaphores(mPresentation.mImages.size(), VK_NULL_HANDLE)
	, mPresentationImageViews(mPresentation.mImages.size(), VK_NULL_HANDLE)
	, mFrameBuffers(mPresentation.mImages.size(), VK_NULL_HANDLE)

	, mQueue(vkengine.mGraphicsQueues.at(0))

	// clang-format off
    , mGeometryMemory()
	, mTriangleMesh{
        .mVertices{
            Vertex{ .position = {+1.0,+1.0,+0.0}, .color = {0.0, 1.0, 0.0} },
            Vertex{ .position = {-1.0,+1.0,+0.0}, .color = {0.0, 1.0, 0.0} },
            Vertex{ .position = {+0.0,-1.0,+0.0}, .color = {0.0, 1.0, 0.0} },
        },
        .mVertexBuffer = blk::Buffer(sizeof(Vertex) * 3, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
    }
// clang-format on
{
	{ // Resources
		mDepthImage.create(mDevice);
		mTriangleMesh.mVertexBuffer.create(mDevice);
	}
	{ // Memories
		auto vkphysicaldevice = *(mDevice.mPhysicalDevice);
		// NOTE(andrea.machizaud) not present on Windows laptop with my NVIDIA card...
		auto memory_type_depth =
			vkphysicaldevice.mMemories.find_compatible(mDepthImage, 0 /*VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT*/);
		auto memory_type_geometry = vkphysicaldevice.mMemories.find_compatible(
			mTriangleMesh.mVertexBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		assert(memory_type_depth);
		assert(memory_type_geometry);
		mDepthMemory = std::make_unique<blk::Memory>(*memory_type_depth, mDepthImage.mRequirements.size);
		mDepthMemory->allocate(mDevice);
		mDepthMemory->bind(mDepthImage);

		mGeometryMemory =
			std::make_unique<blk::Memory>(*memory_type_geometry, mTriangleMesh.mVertexBuffer.mRequirements.size);
		mGeometryMemory->allocate(mDevice);
		mGeometryMemory->bind(mTriangleMesh.mVertexBuffer);
	}
	{ // Image Views
		mDepthImageView = blk::ImageView(mDepthImage, VK_IMAGE_VIEW_TYPE_2D, mDepthFormat, VK_IMAGE_ASPECT_STENCIL_BIT);
		mDepthImageView.create(mDevice);
	}
	{ // Framebuffers
		const VkSemaphoreTypeCreateInfo info_semaphore_type{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.pNext = nullptr,
			.semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
			.initialValue = 0,
		};
		const VkSemaphoreCreateInfo info_semaphore{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &info_semaphore_type,
			.flags = 0,
		};
		for (auto&& [image, view, framebuffer, render_semaphore] :
			 ranges::views::zip(mPresentation.mImages, mPresentationImageViews, mFrameBuffers, mRenderSemaphores))
		{
			const VkImageViewCreateInfo info_imageview{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.image = image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = mColorFormat,
				.components =
					VkComponentMapping{
						.r = VK_COMPONENT_SWIZZLE_R,
						.g = VK_COMPONENT_SWIZZLE_G,
						.b = VK_COMPONENT_SWIZZLE_B,
						.a = VK_COMPONENT_SWIZZLE_A,
					},
				.subresourceRange =
					VkImageSubresourceRange{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
			};
			CHECK(vkCreateImageView(mDevice, &info_imageview, nullptr, &view));

			const std::array attachments{
				view,
				// NOTE(andrea.machizaud) Is it safe to re-use depth here ?
				(VkImageView)mDepthImageView};
			const VkFramebufferCreateInfo info_framebuffer{
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0u,
				.renderPass = mRenderPass,
				.attachmentCount = 1,
				.pAttachments = attachments.data(),
				.width = mResolution.width,
				.height = mResolution.height,
				.layers = 1,
			};
			CHECK(vkCreateFramebuffer(mDevice, &info_framebuffer, nullptr, &framebuffer));

			CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &render_semaphore));
		}
	}
	{ // Commands - Pool
		const VkCommandPoolCreateInfo info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = mQueue->mFamily.mIndex,
		};
		CHECK(vkCreateCommandPool(mDevice, &info, nullptr, &mCommandPool));
	}
	{ // Commands - Buffer
		const VkCommandBufferAllocateInfo info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = mCommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		CHECK(vkAllocateCommandBuffers(mDevice, &info, &mCommandBuffer));
	}
	{ // Synchronisation - Fence
		const VkFenceCreateInfo info{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};
		CHECK(vkCreateFence(mDevice, &info, nullptr, &mFenceRender));
	}
	{ // Synchronisation - Semaphore
		const VkSemaphoreCreateInfo info{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
		};
		CHECK(vkCreateSemaphore(mDevice, &info, nullptr, &mSemaphoreRender));
		CHECK(vkCreateSemaphore(mDevice, &info, nullptr, &mSemaphorePresent));
	}
	{ // Pipeline Layout - Default
		const VkPipelineLayoutCreateInfo info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 0,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr,
		};
		CHECK(vkCreatePipelineLayout(mDevice, &info, nullptr, &mPipelineLayout));
	}
	{ // Pipeline Layout - Mesh
		VkPushConstantRange range{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0,
			.size = sizeof(MeshPushConstants),
		};

		const VkPipelineLayoutCreateInfo info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.setLayoutCount = 0,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &range,
		};
		CHECK(vkCreatePipelineLayout(mDevice, &info, nullptr, &mPipelineLayoutMesh));
	}
	{ // Pipeline
		VkGraphicsPipelineCreateInfo infos[3];
		GraphicPipelineBuilder builder0(
			mDevice,
			mResolution,
			mPipelineLayout,
			mRenderPass,
			0,
			infos[0],
			std::vector<std::uint32_t>(std::begin(kShaderTriangle), std::end(kShaderTriangle)),
			"triangle_main");
		GraphicPipelineBuilder builder1(
			mDevice,
			mResolution,
			mPipelineLayout,
			mRenderPass,
			0,
			infos[1],
			std::vector<std::uint32_t>(std::begin(kShaderDefaultTriangle), std::end(kShaderDefaultTriangle)),
			"default_triangle_main");
		GraphicPipelineBuilderMesh builder2(
			mDevice,
			mResolution,
			mPipelineLayoutMesh,
			mRenderPass,
			0,
			infos[2],
			std::vector<std::uint32_t>(std::begin(kShaderMesh), std::end(kShaderMesh)),
			"mesh_main");
		CHECK(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, std::size(infos), infos, nullptr, mPipelines.data()));
	}
	upload_mesh(mTriangleMesh);
}

Application::~Application()
{
	mTriangleMesh.mVertexBuffer.destroy();
	mGeometryMemory->destroy();

	for (auto&& pipeline : mPipelines)
		vkDestroyPipeline(mDevice, pipeline, nullptr);

	vkDestroyPipelineLayout(mDevice, mPipelineLayoutMesh, nullptr);
	vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);

	vkDestroySemaphore(mDevice, mSemaphorePresent, nullptr);
	vkDestroySemaphore(mDevice, mSemaphoreRender, nullptr);
	vkDestroyFence(mDevice, mFenceRender, nullptr);

	vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

	for (auto&& vksemaphore : mRenderSemaphores)
		vkDestroySemaphore(mDevice, vksemaphore, nullptr);

	for (auto&& vkframebuffer : mFrameBuffers)
		vkDestroyFramebuffer(mDevice, vkframebuffer, nullptr);

	for (auto&& view : mPresentationImageViews)
		vkDestroyImageView(mDevice, view, nullptr);
}

void Application::upload_mesh(blk::Mesh& mesh)
{
	{
		void* data = mTriangleMesh.mVertexBuffer.mMemory->map(mTriangleMesh.mVertexBuffer);
		std::memcpy(data, mTriangleMesh.mVertices.data(), mTriangleMesh.mVertices.size() * sizeof(blk::Vertex));
		mTriangleMesh.mVertexBuffer.mMemory->unmap(mTriangleMesh.mVertexBuffer);
	}
}

void Application::draw()
{
	CHECK(vkWaitForFences(mDevice, 1, &mFenceRender, VK_TRUE, UINT64_MAX));
	CHECK(vkResetFences(mDevice, 1, &mFenceRender));

	std::uint32_t index;
	CHECK(vkAcquireNextImageKHR(
		mDevice, mPresentation.mSwapchain, UINT64_MAX, mSemaphorePresent, VK_NULL_HANDLE, &index));

	CHECK(vkResetCommandBuffer(mCommandBuffer, 0));

	{ // Command - Begin
		constexpr VkCommandBufferBeginInfo info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};
		CHECK(vkBeginCommandBuffer(mCommandBuffer, &info));
	}

	{ // Render Pass - Begin
		float flash = std::abs(std::sinf(mFrameNumber / 120.0f));
		constexpr std::array clear_values{
			VkClearValue{
				.color = VkClearColorValue{.float32 = {0.2f, 0.2f, 0.2f, 1.0f}},
			},
			VkClearValue{
				.depthStencil = VkClearDepthStencilValue{
					.depth = 0.0f,
					.stencil = 0,
				}}};
		const VkRect2D render_area{
			.offset =
				VkOffset2D{
					.x = 0,
					.y = 0,
				},
			.extent = mResolution};

		const VkRenderPassBeginInfo info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = nullptr,
			.renderPass = mRenderPass,
			.framebuffer = mFrameBuffers[index],
			.renderArea = render_area,
			.clearValueCount = static_cast<std::uint32_t>(clear_values.size()),
			.pClearValues = clear_values.data(),
		};
		vkCmdBeginRenderPass(mCommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	vkCmdBindPipeline(mCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[mSelectedShader]);

	switch (mSelectedShader)
	{
	case 0:
	case 1: {
		vkCmdDraw(mCommandBuffer, 3, 1, 0, 0);
		break;
	}
	case 2: {
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(mCommandBuffer, 0, 1, &(mTriangleMesh.mVertexBuffer.mBuffer), &offset);

		{
			glm::vec3 eye{0.0, 0.0, -2.0};
			glm::mat4 view = glm::translate(eye);

			glm::mat4 projection = glm::perspective(glm::radians(70.0), 1700.0 / 900.0, 0.1, 200.0);
			// projection[1][1] *= -1.0;

			glm::mat4 model = glm::rotate(glm::radians(mFrameNumber / 0.4f), glm::vec3(0.0, 1.0, 0.0));

			MeshPushConstants constants{.render_matrix = projection * view * model};
			vkCmdPushConstants(
				mCommandBuffer,
				mPipelineLayoutMesh,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(MeshPushConstants),
				&constants);
		}

		vkCmdDraw(mCommandBuffer, mTriangleMesh.mVertices.size(), 1, 0, 0);
		break;
	}
	default: break;
	}

	vkCmdEndRenderPass(mCommandBuffer);

	CHECK(vkEndCommandBuffer(mCommandBuffer));

	{ // Submit
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		const VkSubmitInfo info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = nullptr,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &mSemaphorePresent,
			.pWaitDstStageMask = &waitStage,
			.commandBufferCount = 1,
			.pCommandBuffers = &mCommandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &mSemaphoreRender,
		};
		CHECK(vkQueueSubmit(*mQueue, 1, &info, mFenceRender));
	}

	{ // Present
		const VkPresentInfoKHR info{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = nullptr,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &mSemaphoreRender,
			.swapchainCount = 1,
			.pSwapchains = &(mPresentation.mSwapchain),
			.pImageIndices = &index,
			.pResults = nullptr,
		};
		const VkResult result_present = vkQueuePresentKHR(*mQueue, &info);
		CHECK(result_present);
		CHECK(
			(result_present == VK_SUCCESS) || (result_present == VK_SUBOPTIMAL_KHR) ||
			(result_present == VK_ERROR_OUT_OF_DATE_KHR));
	}

	++mFrameNumber;
}

} // namespace blk::sample00