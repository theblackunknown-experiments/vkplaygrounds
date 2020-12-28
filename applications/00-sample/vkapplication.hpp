#pragma once

#include <vulkan/vulkan_core.h>

#include "./vkpass.hpp"
#include "./vkrenderpass.hpp"

#include "./vkimage.hpp"
#include "./vkmesh.hpp"

#include <span>
#include <memory>

#include <array>

#include <filesystem>

namespace fs = std::filesystem;

namespace blk
{
struct Engine;
struct Presentation;

struct Device;
struct Memory;
struct Queue;
} // namespace blk

namespace blk::sample00
{
struct Application
{
	// using multipass_type = MultiPass<PassTrait<PassUIOverlay, PassUIOverlay::Arguments>, PassTrait<PassScene, PassScene ::Arguments>>;

	struct RenderPass : ::blk::RenderPass
	{
		RenderPass(Engine& vkengine, VkFormat formatColor, VkFormat formatDepth);
	};

	Engine& mEngine;
	Presentation& mPresentation;
	const Device& mDevice;

	std::uint32_t mFrameNumber{0};
	std::uint32_t mSelectedShader{0};

	VkFormat& mColorFormat;
	VkFormat mDepthFormat;

	VkExtent2D& mResolution;

	RenderPass mRenderPass;
	// multipass_type mMultipass;

	blk::Image mDepthImage;
	blk::ImageView mDepthImageView;

	std::unique_ptr<blk::Memory> mDepthMemory;

	std::vector<VkSemaphore> mRenderSemaphores;
	std::vector<VkImageView> mPresentationImageViews;
	std::vector<VkFramebuffer> mFrameBuffers;

	blk::Queue* mQueue = nullptr;
	VkCommandPool mCommandPool = VK_NULL_HANDLE;
	VkCommandBuffer mCommandBuffer = VK_NULL_HANDLE;

	VkSemaphore mSemaphorePresent = VK_NULL_HANDLE;
	VkSemaphore mSemaphoreRender = VK_NULL_HANDLE;
	VkFence mFenceRender = VK_NULL_HANDLE;

	VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
	VkPipelineLayout mPipelineLayoutMesh = VK_NULL_HANDLE;
	std::array<VkPipeline, 3> mPipelines{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

	std::unique_ptr<blk::Memory> mGeometryMemory;
	blk::Mesh mTriangleMesh;

	Application(blk::Engine& vkengine, blk::Presentation& vkpresentation);
	~Application();

	void load_meshes();

	void upload_mesh(blk::Mesh& mesh);

	void draw();
};
} // namespace blk::sample00
