#pragma once

#include <vulkan/vulkan_core.h>

#include "./vkpass.hpp"
#include "./vkrenderpass.hpp"

#include "./vkimage.hpp"
#include "./vkmesh.hpp"

#include <vkcpumesh.hpp>
#include <vkgpumesh.hpp>
#include <vkcamera.hpp>

#include <span>
#include <memory>

#include <array>
#include <string>
#include <vector>
#include <unordered_map>

#include <functional>

#include <filesystem>

#include <glm/glm.hpp>

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
#if 0
struct Material
{
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
};

struct RenderObject
{
    blk::Mesh* mesh;
    Material* material;
    glm::mat4 transform;
};
#endif

struct MeshStorage
{
    std::unique_ptr<blk::Memory> mGeometryMemory;
    std::unique_ptr<blk::GPUMesh> mGPUMesh;
};

struct Application
{
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

#if 0
	VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
	std::array<VkPipeline, 3> mPipelines{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

    blk::Mesh mTriangleMesh;

    std::unique_ptr<blk::Memory> mUserMemory;
    blk::Mesh mUserMesh;

    std::vector<RenderObject> mRenderables;
    std::unordered_map<std::string, Material> mMaterials;
    std::unordered_map<std::string, Mesh> mMeshes;
#else
    VkPipelineLayout mPipelineLayoutMesh = VK_NULL_HANDLE;
    VkPipeline mPipelineMesh = VK_NULL_HANDLE;
    MeshStorage mStorageMesh;

    DummyCamera mCamera;

    std::vector<std::function<void()>> mPendingRequests;
#endif

    Application(blk::Engine& vkengine, blk::Presentation& vkpresentation);
    ~Application();

#if 0
    void load_meshes();
    void upload_mesh(blk::Mesh& mesh);

    Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
    void record_renderables(VkCommandBuffer commandBuffer, RenderObject* first, std::size_t count);
#else

    void load_mesh(const CPUMesh& mesh);
    void load_pipeline(const char* entry_point, const std::span<const std::uint32_t>& shader);

    void schedule_before_render_command(const std::function<void()>& call) { mPendingRequests.push_back(call); }
#endif

    void draw();
};
} // namespace blk::sample00
