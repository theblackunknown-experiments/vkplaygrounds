#pragma once

#include <vulkan/vulkan_core.h>

#include "./vkpass.hpp"
#include "./vkrenderpass.hpp"

#include "./vkimage.hpp"
#include "./vkmesh.hpp"

#include <vkcpumesh.hpp>
#include <vkgpumesh.hpp>
#include <vkcamera.hpp>
#include <vkmaterial.hpp>
#include <vkrenderable.hpp>

#include <span>
#include <memory>

#include <array>
#include <string>
#include <vector>
#include <unordered_map>
#include <initializer_list>

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
struct MeshStorage
{
    std::unique_ptr<blk::Memory> mGeometryMemory;
    std::array<std::unique_ptr<blk::GPUMesh>, 3> mMeshes;
};

struct MaterialStorage
{
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    std::array<VkPipeline, 3> mPipelines{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::array<blk::Material, 3> mMaterials;
};

struct RenderableStorage
{
    std::size_t mCount = 0;
    std::array<blk::Renderable, 1800> mRenderables;
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

    MeshStorage mStorageMesh;
    MaterialStorage mStorageMaterial;
    RenderableStorage mStorageRenderable;

    DummyCamera mCamera;

    std::vector<std::function<void()>> mPendingRequests;

    Application(blk::Engine& vkengine, blk::Presentation& vkpresentation);
    ~Application();

    void load_mesh(std::uint32_t id, const blk::CPUMesh& mesh);
    void load_meshes(const std::initializer_list<std::uint32_t>& ids, const std::span<const blk::CPUMesh>& meshes);

    void load_pipeline(std::uint32_t id, const char* entry_point, const std::span<const std::uint32_t>& shader);
    void load_pipelines(
        const std::initializer_list<std::uint32_t>& ids,
        const std::span<const char* const>& entry_points,
        const std::span<const std::span<const std::uint32_t>>& shaders);

    void load_renderables(const std::span<const blk::Renderable>& renderables);

    void schedule_before_render_command(const std::function<void()>& call) { mPendingRequests.push_back(call); }

    void draw();
};
} // namespace blk::sample00
