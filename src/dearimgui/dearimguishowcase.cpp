#include "./dearimguishowcase.hpp"

#include "../vkutilities.hpp"
#include "../vkdebug.hpp"

#include "font.hpp"
#include "ui-shader.hpp"
#include "triangle-shader.hpp"

#include <vulkan/vulkan_core.h>

#include <imgui.h>
#include <utilities.hpp>

#include <cassert>
#include <cstddef>
#include <cinttypes>

#include <chrono>
#include <limits>
#include <iterator>

#include <array>
#include <vector>
#include <unordered_map>

namespace
{
    constexpr const bool kVSync = true;

    constexpr const std::uint32_t kSubpassUI = 0;
    constexpr const std::uint32_t kSubpassScene = 1;

    constexpr const std::uint32_t kAttachmentColor = 0;
    constexpr const std::uint32_t kAttachmentDepth = 1;


    constexpr const std::uint32_t kVertexInputBindingPosUVColor = 0;
    constexpr const std::uint32_t kUIShaderLocationPos   = 0;
    constexpr const std::uint32_t kUIShaderLocationUV    = 1;
    constexpr const std::uint32_t kUIShaderLocationColor = 2;

    constexpr const std::uint32_t kShaderBindingFontTexture = 0;

    // TODO(andrea.machizaud) use literals...
    // TODO(andrea.machizaud) pre-allocate a reasonable amount for buffers
    constexpr const std::size_t kInitialVertexBufferSize = 2 << 20; // 1 Mb
    constexpr const std::size_t kInitialIndexBufferSize  = 2 << 20; // 1 Mb

    struct alignas(4) DearImGuiConstants {
        float scale    [2];
        float translate[2];
    };
}

DearImGuiShowcase::DearImGuiShowcase(
    VkInstance vkinstance,
    VkSurfaceKHR vksurface,
    VkPhysicalDevice vkphysicaldevice,
    const VkExtent2D& resolution)
    : mInstance(vkinstance)
    , mSurface(vksurface)
    , mPhysicalDevice(vkphysicaldevice)
    , mResolution(resolution)
    , mContext(ImGui::CreateContext())
{
    vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mFeatures);
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &mProperties);
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProperties);
    {// Queue Family Properties
        std::uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &count, nullptr);
        assert(count > 0);
        mQueueFamiliesProperties.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &count, mQueueFamiliesProperties.data());
    }
    {// Extensions
        std::uint32_t count;
        CHECK(vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &count, nullptr));
        mExtensions.resize(count);
        CHECK(vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &count, mExtensions.data()));
    }
    {// Queue Family
        std::uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &count, nullptr);
        assert(count > 0);

        std::vector<VkQueueFamilyProperties> queue_families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &count, queue_families.data());

        EnumerateIterator begin(std::begin(queue_families));
        EnumerateIterator end(std::end(queue_families));

        auto finderSuitableQueueFamily = std::find_if(
            begin, end,
            [vkphysicaldevice, vksurface](const std::tuple<std::uint32_t, VkQueueFamilyProperties>& data) {
                const std::uint32_t&           index      = std::get<0>(data);
                const VkQueueFamilyProperties& properties = std::get<1>(data);

                #ifdef OS_WINDOWS
                if (!vkGetPhysicalDeviceWin32PresentationSupportKHR(vkphysicaldevice, index))
                    return false;
                #endif

                VkBool32 supported;
                CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vkphysicaldevice, index, vksurface, &supported));
                return supported
                    && (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT);
            }
        );

        mQueueFamily = std::distance(begin, finderSuitableQueueFamily);
    }
    {// Device
        constexpr const VkPhysicalDeviceFeatures features{
            .robustBufferAccess                      = VK_FALSE,
            .fullDrawIndexUint32                     = VK_FALSE,
            .imageCubeArray                          = VK_FALSE,
            .independentBlend                        = VK_FALSE,
            .geometryShader                          = VK_FALSE,
            .tessellationShader                      = VK_FALSE,
            .sampleRateShading                       = VK_FALSE,
            .dualSrcBlend                            = VK_FALSE,
            .logicOp                                 = VK_FALSE,
            .multiDrawIndirect                       = VK_FALSE,
            .drawIndirectFirstInstance               = VK_FALSE,
            .depthClamp                              = VK_FALSE,
            .depthBiasClamp                          = VK_FALSE,
            .fillModeNonSolid                        = VK_FALSE,
            .depthBounds                             = VK_FALSE,
            .wideLines                               = VK_FALSE,
            .largePoints                             = VK_FALSE,
            .alphaToOne                              = VK_FALSE,
            .multiViewport                           = VK_FALSE,
            .samplerAnisotropy                       = VK_FALSE,
            .textureCompressionETC2                  = VK_FALSE,
            .textureCompressionASTC_LDR              = VK_FALSE,
            .textureCompressionBC                    = VK_FALSE,
            .occlusionQueryPrecise                   = VK_FALSE,
            .pipelineStatisticsQuery                 = VK_FALSE,
            .vertexPipelineStoresAndAtomics          = VK_FALSE,
            .fragmentStoresAndAtomics                = VK_FALSE,
            .shaderTessellationAndGeometryPointSize  = VK_FALSE,
            .shaderImageGatherExtended               = VK_FALSE,
            .shaderStorageImageExtendedFormats       = VK_FALSE,
            .shaderStorageImageMultisample           = VK_FALSE,
            .shaderStorageImageReadWithoutFormat     = VK_FALSE,
            .shaderStorageImageWriteWithoutFormat    = VK_FALSE,
            .shaderUniformBufferArrayDynamicIndexing = VK_FALSE,
            .shaderSampledImageArrayDynamicIndexing  = VK_FALSE,
            .shaderStorageBufferArrayDynamicIndexing = VK_FALSE,
            .shaderStorageImageArrayDynamicIndexing  = VK_FALSE,
            .shaderClipDistance                      = VK_FALSE,
            .shaderCullDistance                      = VK_FALSE,
            .shaderFloat64                           = VK_FALSE,
            .shaderInt64                             = VK_FALSE,
            .shaderInt16                             = VK_FALSE,
            .shaderResourceResidency                 = VK_FALSE,
            .shaderResourceMinLod                    = VK_FALSE,
            .sparseBinding                           = VK_FALSE,
            .sparseResidencyBuffer                   = VK_FALSE,
            .sparseResidencyImage2D                  = VK_FALSE,
            .sparseResidencyImage3D                  = VK_FALSE,
            .sparseResidency2Samples                 = VK_FALSE,
            .sparseResidency4Samples                 = VK_FALSE,
            .sparseResidency8Samples                 = VK_FALSE,
            .sparseResidency16Samples                = VK_FALSE,
            .sparseResidencyAliased                  = VK_FALSE,
            .variableMultisampleRate                 = VK_FALSE,
            .inheritedQueries                        = VK_FALSE,
        };
        constexpr const std::array<const char*, 1> kEnabledExtensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };
        constexpr const float kQueuePriorities = 1.0f;
        const VkDeviceQueueCreateInfo info_queue = VkDeviceQueueCreateInfo{
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .queueFamilyIndex = mQueueFamily,
            .queueCount       = 1,
            .pQueuePriorities = &kQueuePriorities,
        };
        const VkDeviceCreateInfo info{
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .queueCreateInfoCount    = 1,
            .pQueueCreateInfos       = &info_queue,
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            .enabledExtensionCount   = kEnabledExtensions.size(),
            .ppEnabledExtensionNames = kEnabledExtensions.data(),
            .pEnabledFeatures        = &features,
        };
        CHECK(vkCreateDevice(mPhysicalDevice, &info, nullptr, &mDevice));
        vkGetDeviceQueue(mDevice, mQueueFamily, 0, &mQueue);
    }
    {// Dear ImGui
        IMGUI_CHECKVERSION();
        {// Style
            ImGuiStyle& style = ImGui::GetStyle();
            ImGui::StyleColorsDark(&style);
            // ImGui::StyleColorsClassic(&style);
            // ImGui::StyleColorsLight(&style);
        }
        {// IO
            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize             = ImVec2(mResolution.width, mResolution.height);
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
            io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
            io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
            io.BackendPlatformName = "Win32";
            io.BackendRendererName = "vkplaygrounds";
            {// Font
                ImFontConfig config;
                std::strncpy(config.Name, kFontName, (sizeof(config.Name) / sizeof(config.Name[0])) - 1);
                config.FontDataOwnedByAtlas = false;
                io.Fonts->AddFontFromMemoryTTF(
                    const_cast<unsigned char*>(&kFont[0]), sizeof(kFont),
                    kFontSizePixels,
                    &config
                );
            }
        }
    }
}

DearImGuiShowcase::~DearImGuiShowcase()
{
    for(auto&& vkpipeline : mPipelines)
        vkDestroyPipeline(mDevice, vkpipeline, nullptr);

    vkFreeCommandBuffers(mDevice, mCommandPool, mCommandBuffers.size(), mCommandBuffers.data());

    for(auto&& vkframebuffer : mFrameBuffers)
        vkDestroyFramebuffer(mDevice, vkframebuffer, nullptr);

    for(auto&& view : mPresentation.views)
        vkDestroyImageView(mDevice, view, nullptr);
    vkDestroySwapchainKHR(mDevice, mPresentation.swapchain, nullptr);

    for(auto&& chunk : mMemoryChunks)
        vkFreeMemory(mDevice, chunk.memory, nullptr);

    vkDestroyBuffer(mDevice, mStaging.buffer, nullptr);
    vkDestroyBuffer(mDevice, mIndexBuffer.buffer, nullptr);
    vkDestroyBuffer(mDevice, mVertexBuffer.buffer, nullptr);

    vkDestroyImageView(mDevice, mFont.view, nullptr);
    vkDestroyImage(mDevice, mFont.image, nullptr);

    vkDestroyImageView(mDevice, mDepth.view, nullptr);
    vkDestroyImage(mDevice, mDepth.image, nullptr);

    vkFreeCommandBuffers(mDevice, mCommandPoolOneOff, 1, &mStagingCommandBuffer);

    vkDestroyFence(mDevice, mStagingFence, nullptr);

    vkDestroySemaphore(mDevice, mStagingSemaphore, nullptr);
    vkDestroySemaphore(mDevice, mRenderSemaphore, nullptr);
    vkDestroySemaphore(mDevice, mAcquiredSemaphore, nullptr);

    vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
    vkDestroyCommandPool(mDevice, mCommandPoolOneOff, nullptr);
    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
    vkDestroyPipelineLayout(mDevice, mUIPipelineLayout, nullptr);
    vkDestroyPipelineLayout(mDevice, mScenePipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);

    vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

    vkDestroySampler(mDevice, mSampler, nullptr);

    vkDestroyDevice(mDevice, nullptr);

    ImGui::DestroyContext(mContext);
}

bool DearImGuiShowcase::isSuitable(VkPhysicalDevice vkphysicaldevice, VkSurfaceKHR vksurface)
{
    // Engine requirements:
    //  - GRAPHICS queue
    // Presentation requirements:
    //  - Surface compatible queue

    VkPhysicalDeviceProperties vkproperties;
    vkGetPhysicalDeviceProperties(vkphysicaldevice, &vkproperties);
    if (vkproperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        return false;

    std::uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
    assert(count > 0);

    std::vector<VkQueueFamilyProperties> queue_families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, queue_families.data());

    EnumerateIterator begin(std::begin(queue_families));
    EnumerateIterator end(std::end(queue_families));

    auto finderSuitableGraphicsQueue = std::find_if(
        begin, end,
        [vkphysicaldevice, vksurface](const std::tuple<std::uint32_t, VkQueueFamilyProperties>& data) {
            const std::uint32_t&           index      = std::get<0>(data);
            const VkQueueFamilyProperties& properties = std::get<1>(data);

            #ifdef OS_WINDOWS
            if (!vkGetPhysicalDeviceWin32PresentationSupportKHR(vkphysicaldevice, index))
                return false;
            #endif

            VkBool32 supported;
            CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(vkphysicaldevice, index, vksurface, &supported));
            return supported
                && (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT);
        }
    );

    return finderSuitableGraphicsQueue != end;
}

void DearImGuiShowcase::initialize()
{
    {// Present Modes
        mPresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
        if(kVSync)
        {
            // guaranteed by spec
            mPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        }
        else
        {
            std::uint32_t count;
            CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &count, nullptr));
            assert(count > 0);

            std::vector<VkPresentModeKHR> present_modes(count);
            CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &count, present_modes.data()));

            constexpr const auto kPreferredPresentModes = {
                VK_PRESENT_MODE_MAILBOX_KHR,
                VK_PRESENT_MODE_IMMEDIATE_KHR,
                VK_PRESENT_MODE_FIFO_KHR,
            };

            for (auto&& preferred : kPreferredPresentModes)
            {
                auto finder_present_mode = std::find(
                    std::begin(present_modes), std::end(present_modes),
                    preferred
                );

                if (finder_present_mode != std::end(present_modes))
                {
                    mPresentMode = *finder_present_mode;
                }
            }
            assert(mPresentMode != VK_PRESENT_MODE_MAX_ENUM_KHR);
        }
    }
    {// Formats
        {// Color Attachment
            // Simple case : we target the same format as the presentation capabilities
            std::uint32_t count;
            vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &count, nullptr);
            assert(count > 0);

            std::vector<VkSurfaceFormatKHR> formats(count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &count, formats.data());

            // NOTE(andrea.machizaud) Arbitrary : it happens to be supported on my device
            constexpr const VkFormat kPreferredFormat = VK_FORMAT_B8G8R8A8_UNORM;
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
            constexpr const VkFormat kDEPTH_FORMATS[] = {
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
    {// Sampler
        constexpr const VkSamplerCreateInfo info{
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
    {// Render Pass
        // Pass 0 : Draw UI    (write depth)
        // Pass 1 : Draw Scene (read depth, write color)
        //  Depth discarded
        //  Color kept
        //  Transition to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR to optimize transition before presentation
        const std::array<VkAttachmentDescription, 2> attachments{
            VkAttachmentDescription{
                .flags          = 0,
                .format         = mColorAttachmentFormat,
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            VkAttachmentDescription{
                .flags          = 0,
                .format         = mDepthStencilAttachmentFormat,
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            },
        };
        constexpr const VkAttachmentReference color_reference{
            .attachment = kAttachmentColor,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        constexpr const VkAttachmentReference depth_reference{
            .attachment = kAttachmentDepth,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        constexpr const std::array<VkSubpassDescription, 2> subpasses{
            // Pass 0 : Draw UI    (write depth, write color)
            VkSubpassDescription{
                .flags                   = 0,
                .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount    = 0,
                .pInputAttachments       = nullptr,
                .colorAttachmentCount    = 1,
                .pColorAttachments       = &color_reference,
                .pResolveAttachments     = nullptr,
                .pDepthStencilAttachment = &depth_reference,
                .preserveAttachmentCount = 0,
                .pPreserveAttachments    = nullptr,
            },
            // Pass 1 : Draw Scene (read depth, write color)
            VkSubpassDescription{
                .flags                   = 0,
                .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount    = 0,
                .pInputAttachments       = nullptr,
                .colorAttachmentCount    = 1,
                .pColorAttachments       = &color_reference,
                .pResolveAttachments     = nullptr,
                .pDepthStencilAttachment = nullptr,
                // .pDepthStencilAttachment = &depth_reference,
                .preserveAttachmentCount = 0,
                .pPreserveAttachments    = nullptr,
            },
        };
        constexpr const std::array<VkSubpassDependency, 1> dependencies{
            VkSubpassDependency{
                .srcSubpass      = kSubpassUI,
                .dstSubpass      = kSubpassScene,
                .srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT ,
                .dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT ,
                .srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
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
        CHECK(vkCreateRenderPass(mDevice, &info, nullptr, &mRenderPass));
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
        {// UI
            constexpr std::size_t kAlignOf = alignof(DearImGuiConstants);
            constexpr std::size_t kMaxAlignOf = alignof(std::max_align_t);
            constexpr std::size_t kSizeOf = sizeof(DearImGuiConstants);

            constexpr const std::array<VkPushConstantRange, 1> ranges{
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
                .pushConstantRangeCount = ranges.size(),
                .pPushConstantRanges    = ranges.data(),
            };
            CHECK(vkCreatePipelineLayout(mDevice, &info, nullptr, &mUIPipelineLayout));
        }
        {// Scene
            const VkPipelineLayoutCreateInfo info{
                .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext                  = nullptr,
                .flags                  = 0,
                .setLayoutCount         = 0,
                .pSetLayouts            = nullptr,
                .pushConstantRangeCount = 0,
                .pPushConstantRanges    = nullptr,
            };
            CHECK(vkCreatePipelineLayout(mDevice, &info, nullptr, &mScenePipelineLayout));
        }
    }
    {// Pipeline Cache
        const VkPipelineCacheCreateInfo info{
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0u,
            // TODO
            .initialDataSize = 0u,
            .pInitialData    = nullptr,
        };
        CHECK(vkCreatePipelineCache(mDevice, &info, nullptr, &mPipelineCache));
    }
    {// Pools
        {// Command Pools
            const VkCommandPoolCreateInfo info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = mQueueFamily,
            };
            CHECK(vkCreateCommandPool(mDevice, &info, nullptr, &mCommandPool));
        }
        {// Command Pools - One Off
            const VkCommandPoolCreateInfo info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                .queueFamilyIndex = mQueueFamily,
            };
            CHECK(vkCreateCommandPool(mDevice, &info, nullptr, &mCommandPoolOneOff));
        }
        {// Descriptor Pools
            constexpr const std::uint32_t kMaxAllocatedSets = 1;
            constexpr const std::array<VkDescriptorPoolSize, 1> pool_sizes{
                // 1 sampler : font texture
                VkDescriptorPoolSize{
                    .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                }
            };
            constexpr const VkDescriptorPoolCreateInfo info{
                .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext         = nullptr,
                .flags         = 0,
                .maxSets       = kMaxAllocatedSets,
                .poolSizeCount = pool_sizes.size(),
                .pPoolSizes    = pool_sizes.data(),
            };
            CHECK(vkCreateDescriptorPool(mDevice, &info, nullptr, &mDescriptorPool));
        }
    }
    {// Semaphores
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
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mAcquiredSemaphore));
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mRenderSemaphore));
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mStagingSemaphore));
    }
    {// Fences
        const VkFenceCreateInfo info{
            .sType              = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext              = nullptr,
            .flags              = 0,
        };
        CHECK(vkCreateFence(mDevice, &info, nullptr, &mStagingFence));
    }
    {// Command Buffers
        const VkCommandBufferAllocateInfo info{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = mCommandPoolOneOff,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        CHECK(vkAllocateCommandBuffers(mDevice, &info, &mStagingCommandBuffer));
    }
}

void DearImGuiShowcase::initialize_resources()
{
    // NVIDIA - https://developer.nvidia.com/blog/vulkan-dos-donts/
    //  > Parallelize command buffer recording, image and buffer creation, descriptor set updates, pipeline creation, and memory allocation / binding. Task graph architecture is a good option which allows sufficient parallelism in terms of draw submission while also respecting resource and command queue dependencies.
    {// Images
        {// Font
            ImGuiIO& io = ImGui::GetIO();
            {
                int width = 0, height = 0;
                unsigned char* data = nullptr;
                io.Fonts->GetTexDataAsAlpha8(&data, &width, &height);

                const VkImageCreateInfo info{
                    .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                    .pNext                 = nullptr,
                    .flags                 = 0,
                    .imageType             = VK_IMAGE_TYPE_2D,
                    .format                = VK_FORMAT_R8_UNORM,
                    .extent                = VkExtent3D{
                        .width  = static_cast<std::uint32_t>(width),
                        .height = static_cast<std::uint32_t>(height),
                        .depth  = 1,
                    },
                    .mipLevels             = 1,
                    .arrayLayers           = 1,
                    .samples               = VK_SAMPLE_COUNT_1_BIT,
                    .tiling                = VK_IMAGE_TILING_OPTIMAL,
                    .usage                 = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                    .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                    .queueFamilyIndexCount = 1,
                    .pQueueFamilyIndices   = &mQueueFamily,
                    .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
                };
                CHECK(vkCreateImage(mDevice, &info, nullptr, &mFont.image));
                vkGetImageMemoryRequirements(mDevice, mFont.image, &mFont.requirements);
            }
            io.Fonts->SetTexID(&mFont);
        }
        {// Depth
            const VkImageCreateInfo info{
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = mDepthStencilAttachmentFormat,
                .extent                = VkExtent3D{
                    .width  = mResolution.width,
                    .height = mResolution.height,
                    .depth  = 1,
                },
                .mipLevels             = 1,
                .arrayLayers           = 1,
                .samples               = VK_SAMPLE_COUNT_1_BIT,
                .tiling                = VK_IMAGE_TILING_OPTIMAL,
                .usage                 = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
                .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
            };
            CHECK(vkCreateImage(mDevice, &info, nullptr, &mDepth.image));
            vkGetImageMemoryRequirements(mDevice, mDepth.image, &mDepth.requirements);
        }
    }
    {// Buffers
        {// Vertex
            constexpr const VkBufferCreateInfo info{
                .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .size                  = kInitialVertexBufferSize,
                .usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
            };
            CHECK(vkCreateBuffer(mDevice, &info, nullptr, &mVertexBuffer.buffer));
            vkGetBufferMemoryRequirements(mDevice, mVertexBuffer.buffer, &mVertexBuffer.requirements);
        }
        {// Vertex
            constexpr const VkBufferCreateInfo info{
                .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .size                  = kInitialIndexBufferSize,
                .usage                 = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
            };
            CHECK(vkCreateBuffer(mDevice, &info, nullptr, &mIndexBuffer.buffer));
            vkGetBufferMemoryRequirements(mDevice, mIndexBuffer.buffer, &mIndexBuffer.requirements);
        }
        {// Staging
            constexpr const VkBufferCreateInfo info{
                .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .size                  = kInitialIndexBufferSize,
                .usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
            };
            CHECK(vkCreateBuffer(mDevice, &info, nullptr, &mStaging.buffer));
            vkGetBufferMemoryRequirements(mDevice, mStaging.buffer, &mStaging.requirements);
        }
    }
}

void DearImGuiShowcase::initialize_views()
{
    {// Images
        {// Font
            const VkImageViewCreateInfo info{
                .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .image            = mFont.image,
                .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                .format           = VK_FORMAT_R8_UNORM,
                .components       = VkComponentMapping{
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
                .subresourceRange = VkImageSubresourceRange{
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
            };
            CHECK(vkCreateImageView(mDevice, &info, nullptr, &mFont.view));
        }
        {// Depth
            VkImageAspectFlags aspects = VK_IMAGE_ASPECT_DEPTH_BIT;
            if(mDepthStencilAttachmentFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
                aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
            const VkImageViewCreateInfo info{
                .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .image            = mDepth.image,
                .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                .format           = mDepthStencilAttachmentFormat,
                .components       = VkComponentMapping{
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
                .subresourceRange = VkImageSubresourceRange{
                    .aspectMask     = aspects,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
            };
            CHECK(vkCreateImageView(mDevice, &info, nullptr, &mDepth.view));
        }
    }
}

void DearImGuiShowcase::allocate_memory()
{
    auto memory_gpu_font = get_memory_type(
        mMemoryProperties, mFont.requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    auto memory_gpu_depth = get_memory_type(
        mMemoryProperties, mDepth.requirements.memoryTypeBits,
        // NOTE(andrea.machizaud) not present on Windows laptop with my NVIDIA card...
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT /*| VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT*/
    );
    auto memory_cpu_vertex = get_memory_type(
        mMemoryProperties, mVertexBuffer.requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    auto memory_cpu_index = get_memory_type(
        mMemoryProperties, mIndexBuffer.requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    auto memory_cpu_staging = get_memory_type(
        mMemoryProperties, mStaging.requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    assert(memory_gpu_font.has_value());
    assert(memory_gpu_depth.has_value());
    assert(memory_cpu_vertex.has_value());
    assert(memory_cpu_index.has_value());
    assert(memory_cpu_staging.has_value());

    std::unordered_map<std::uint32_t, VkDeviceSize> packed_sizes;
    packed_sizes[memory_gpu_font   .value()] += mFont.requirements.size;
    packed_sizes[memory_gpu_depth  .value()] += mDepth.requirements.size;
    packed_sizes[memory_cpu_vertex .value()] += mVertexBuffer.requirements.size;
    packed_sizes[memory_cpu_index  .value()] += mIndexBuffer.requirements.size;
    packed_sizes[memory_cpu_staging.value()] += mStaging.requirements.size;

    mMemoryChunks.resize(packed_sizes.size());
    for(auto&& [chunk, entry] : ZipRange(mMemoryChunks, packed_sizes))
    {
        const VkMemoryAllocateInfo info{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = entry.second,
            .memoryTypeIndex = entry.first,
        };
        CHECK(vkAllocateMemory(mDevice, &info, nullptr, &chunk.memory));
        chunk.memory_type_index = info.memoryTypeIndex;
        chunk.free = info.allocationSize;
    }
    for (auto&& [idx, chunk] : EnumerateRange(mMemoryChunks))
    {
        if (chunk.memory_type_index == memory_gpu_font.value())
            mFont.memory_chunk_index = idx;
        if (chunk.memory_type_index == memory_gpu_depth.value())
            mDepth.memory_chunk_index = idx;
        if (chunk.memory_type_index == memory_cpu_vertex.value())
            mVertexBuffer.memory_chunk_index = idx;
        if (chunk.memory_type_index == memory_cpu_index.value())
            mIndexBuffer.memory_chunk_index = idx;
        if (chunk.memory_type_index == memory_cpu_staging.value())
            mStaging.memory_chunk_index = idx;
    }
}

void DearImGuiShowcase::allocate_descriptorset()
{
    const VkDescriptorSetAllocateInfo info{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext              = nullptr,
        .descriptorPool     = mDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = &mDescriptorSetLayout,
    };
    CHECK(vkAllocateDescriptorSets(mDevice, &info, &mDescriptorSet));
}

void DearImGuiShowcase::upload_font_image()
{
    ImGuiIO& io = ImGui::GetIO();
    int width = 0, height = 0;
    unsigned char* data = nullptr;
    io.Fonts->GetTexDataAsAlpha8(&data, &width, &height);

    // Simply to illustrate operation are deferred unti submission, record commandbuffer first
    {// Staging Command Buffer Recording
        {// Start
            const VkCommandBufferBeginInfo info{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext            = nullptr,
                .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                .pInheritanceInfo = nullptr,
            };
            CHECK(vkBeginCommandBuffer(mStagingCommandBuffer, &info));
        }
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
                .image               = mFont.image,
                .subresourceRange    = VkImageSubresourceRange{
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
            };
            vkCmdPipelineBarrier(
                mStagingCommandBuffer,
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
                .imageExtent = VkExtent3D{
                    .width  = static_cast<std::uint32_t>(width),
                    .height = static_cast<std::uint32_t>(height),
                    .depth  = 1,
                },
            };
            vkCmdCopyBufferToImage(
                mStagingCommandBuffer,
                mStaging.buffer,
                mFont.image,
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
                .image               = mFont.image,
                .subresourceRange    = VkImageSubresourceRange{
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                }
            };
            vkCmdPipelineBarrier(
                mStagingCommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &imagebarrier
            );
        }
        CHECK(vkEndCommandBuffer(mStagingCommandBuffer));
    }

    {// Memory mapping
        // NOTE(andrea.machizaud) Coherent memory no invalidate/flush
        unsigned char* mapped_address = nullptr;
        CHECK(vkMapMemory(
            mDevice,
            mMemoryChunks.at(mStaging.memory_chunk_index).memory,
            mStaging.offset,
            mStaging.requirements.size,
            0,
            reinterpret_cast<void**>(&mapped_address)
        ));

        std::copy_n(data, width * height, mapped_address);
        vkUnmapMemory(mDevice, mMemoryChunks.at(mStaging.memory_chunk_index).memory);
        // Occupiped to 0 once submit completed
        mStaging.occupied = width * height * sizeof(unsigned char);
    }
    {// Submission
        const VkSubmitInfo info{
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 0,
            .pWaitSemaphores      = nullptr,
            .pWaitDstStageMask    = nullptr,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &mStagingCommandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = &mStagingSemaphore,
        };
        CHECK(vkQueueSubmit(mQueue, 1, &info, mStagingFence));
    }
}

void DearImGuiShowcase::create_swapchain()
{
    VkSurfaceCapabilitiesKHR capabilities;
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &capabilities));

    mResolution = (capabilities.currentExtent.width == std::numeric_limits<std::uint32_t>::max())
        ? mResolution
        : capabilities.currentExtent;

    {// Creation
        VkSwapchainKHR previous_swapchain = mPresentation.swapchain;

        const std::uint32_t image_count = (capabilities.maxImageCount > 0)
            ? std::min(capabilities.minImageCount + 1, capabilities.maxImageCount)
            : capabilities.minImageCount + 1;

        const VkSurfaceTransformFlagBitsKHR transform = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
            : capabilities.currentTransform;

        constexpr const VkCompositeAlphaFlagBitsKHR kPreferredCompositeAlphaFlags[] = {
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        auto finder_composite_alpha = std::find_if(
            std::begin(kPreferredCompositeAlphaFlags), std::end(kPreferredCompositeAlphaFlags),
            [&capabilities](const VkCompositeAlphaFlagBitsKHR& f) {
                return capabilities.supportedCompositeAlpha & f;
            }
        );
        assert(finder_composite_alpha != std::end(kPreferredCompositeAlphaFlags));

        const VkCompositeAlphaFlagBitsKHR composite_alpha = *finder_composite_alpha;

        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // be greedy
        if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        // be greedy
        if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        const VkSwapchainCreateInfoKHR info{
            .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext                 = nullptr,
            .flags                 = 0,
            .surface               = mSurface,
            .minImageCount         = image_count,
            .imageFormat           = mColorAttachmentFormat,
            .imageColorSpace       = mColorSpace,
            .imageExtent           = mResolution,
            .imageArrayLayers      = 1,
            .imageUsage            = usage,
            .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .preTransform          = transform,
            .compositeAlpha        = composite_alpha,
            .presentMode           = mPresentMode,
            .clipped               = VK_TRUE,
            .oldSwapchain          = previous_swapchain,
        };
        CHECK(vkCreateSwapchainKHR(mDevice, &info, nullptr, &(mPresentation.swapchain)));

        if (previous_swapchain != VK_NULL_HANDLE)
        {// Cleaning
            for(auto&& view : mPresentation.views)
                vkDestroyImageView(mDevice, view, nullptr);
            vkDestroySwapchainKHR(mDevice, previous_swapchain, nullptr);
        }
    }
    {// Views
        std::uint32_t count;
        CHECK(vkGetSwapchainImagesKHR(mDevice, mPresentation.swapchain, &count, nullptr));
        mPresentation.images.resize(count);
        mPresentation.views.resize(count);
        CHECK(vkGetSwapchainImagesKHR(mDevice, mPresentation.swapchain, &count, mPresentation.images.data()));

        for (auto&& [image, view] : ZipRange(mPresentation.images, mPresentation.views))
        {
            const VkImageViewCreateInfo info{
                .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .image            = image,
                .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                .format           = mColorAttachmentFormat,
                .components       = VkComponentMapping{
                    .r = VK_COMPONENT_SWIZZLE_R,
                    .g = VK_COMPONENT_SWIZZLE_G,
                    .b = VK_COMPONENT_SWIZZLE_B,
                    .a = VK_COMPONENT_SWIZZLE_A,
                },
                .subresourceRange = VkImageSubresourceRange{
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1,
                },
            };
            CHECK(vkCreateImageView(mDevice, &info, nullptr, &view));
        }
    }
}

void DearImGuiShowcase::create_framebuffers()
{
    mFrameBuffers.resize(mPresentation.views.size());
    for (auto&& [vkview, vkframebuffer] : ZipRange(mPresentation.views, mFrameBuffers))
    {
        const std::array<VkImageView, 2> attachments{
            vkview,
            // NOTE(andrea.machizaud) Is it safe to re-use depth here ?
            mDepth.view
        };
        const VkFramebufferCreateInfo info{
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0u,
            .renderPass      = mRenderPass,
            .attachmentCount = attachments.size(),
            .pAttachments    = attachments.data(),
            .width           = mResolution.width,
            .height          = mResolution.height,
            .layers          = 1,
        };
        CHECK(vkCreateFramebuffer(mDevice, &info, nullptr, &vkframebuffer));
    }
}

void DearImGuiShowcase::create_commandbuffers()
{
    mCommandBuffers.resize(mPresentation.views.size());
    const VkCommandBufferAllocateInfo info{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = mCommandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<std::uint32_t>(mCommandBuffers.size()),
    };
    CHECK(vkAllocateCommandBuffers(mDevice, &info, mCommandBuffers.data()));
}

void DearImGuiShowcase::create_graphic_pipelines()
{
    VkShaderModule shader_ui = VK_NULL_HANDLE;
    VkShaderModule shader_triangle = VK_NULL_HANDLE;

    {// Shader - UI
        constexpr const VkShaderModuleCreateInfo info{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = kShaderUI.size() * sizeof(std::uint32_t),
            .pCode    = kShaderUI.data(),
        };
        CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &shader_ui));
    }
    {// Shader - Triangle
        constexpr const VkShaderModuleCreateInfo info{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = kShaderTriangle.size() * sizeof(std::uint32_t),
            .pCode    = kShaderTriangle.data(),
        };
        CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &shader_triangle));
    }

    const std::array<VkPipelineShaderStageCreateInfo, 2> uistages{
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
    const std::array<VkPipelineShaderStageCreateInfo, 2> trianglestages{
        VkPipelineShaderStageCreateInfo{
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_VERTEX_BIT,
            .module              = shader_triangle,
            .pName               = "triangle_main",
            .pSpecializationInfo = nullptr,
        },
        VkPipelineShaderStageCreateInfo{
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module              = shader_triangle,
            .pName               = "triangle_main",
            .pSpecializationInfo = nullptr,
        },
    };

    constexpr const std::array<VkVertexInputBindingDescription, 1> vertex_bindings{
        VkVertexInputBindingDescription
        {
            .binding   = kVertexInputBindingPosUVColor,
            .stride    = sizeof(ImDrawVert),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }
    };

    constexpr const std::array<VkVertexInputAttributeDescription, 3> vertex_attributes{
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

    constexpr const VkPipelineVertexInputStateCreateInfo uivertexinput{
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                           = 0,
        .vertexBindingDescriptionCount   = vertex_bindings.size(),
        .pVertexBindingDescriptions      = vertex_bindings.data(),
        .vertexAttributeDescriptionCount = vertex_attributes.size(),
        .pVertexAttributeDescriptions    = vertex_attributes.data(),
    };

    constexpr const VkPipelineVertexInputStateCreateInfo scenevertexinput{
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                           = 0,
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = nullptr,
    };

    constexpr const VkPipelineInputAssemblyStateCreateInfo assembly{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext                  = nullptr,
        .flags                  = 0,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    constexpr const VkPipelineViewportStateCreateInfo uiviewport{
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .viewportCount = 1,
        .pViewports    = nullptr, // dynamic state
        .scissorCount  = 1,
        .pScissors     = nullptr, // dynamic state
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
    const VkPipelineViewportStateCreateInfo sceneviewport{
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext         = nullptr,
        .flags         = 0,
        .viewportCount = 1,
        .pViewports    = &fullviewport,
        .scissorCount  = 1,
        .pScissors     = &fullscissors,
    };

    constexpr const VkPipelineRasterizationStateCreateInfo rasterization{
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

    constexpr const VkPipelineMultisampleStateCreateInfo multisample{
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

    constexpr const VkPipelineDepthStencilStateCreateInfo uidepthstencil{
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .depthTestEnable       = VK_TRUE,
        .depthWriteEnable      = VK_TRUE,
        .depthCompareOp        = VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
    };

    constexpr const VkPipelineDepthStencilStateCreateInfo scenedepthstencil{
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .depthTestEnable       = VK_FALSE,
        .depthWriteEnable      = VK_FALSE,
        .depthCompareOp        = VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
    };

    constexpr const std::array<VkPipelineColorBlendAttachmentState, 1> colorblendattachments_passthrough{
        VkPipelineColorBlendAttachmentState{
            .blendEnable         = VK_FALSE,
            .colorWriteMask      =
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT,
        }
    };

    constexpr const std::array<VkPipelineColorBlendAttachmentState, 1> colorblendattachments_blend{
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

    constexpr const VkPipelineColorBlendStateCreateInfo uicolorblend{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .logicOpEnable           = VK_FALSE,
        .logicOp                 = VK_LOGIC_OP_CLEAR,
        .attachmentCount         = colorblendattachments_passthrough.size(),
        .pAttachments            = colorblendattachments_passthrough.data(),
    };

    constexpr const VkPipelineColorBlendStateCreateInfo scenecolorblend{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .logicOpEnable           = VK_FALSE,
        .logicOp                 = VK_LOGIC_OP_COPY,
        .attachmentCount         = colorblendattachments_blend.size(),
        .pAttachments            = colorblendattachments_blend.data(),
        .blendConstants          = { 0.0f, 0.0f, 0.0f, 0.0f },
    };

    constexpr const std::array<VkDynamicState, 2> states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    constexpr const VkPipelineDynamicStateCreateInfo uidynamics{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .dynamicStateCount       = states.size(),
        .pDynamicStates          = states.data(),
    };

    const std::array<VkGraphicsPipelineCreateInfo, 2> infos{
        VkGraphicsPipelineCreateInfo
        {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stageCount          = uistages.size(),
            .pStages             = uistages.data(),
            .pVertexInputState   = &uivertexinput,
            .pInputAssemblyState = &assembly,
            .pTessellationState  = nullptr,
            .pViewportState      = &uiviewport,
            .pRasterizationState = &rasterization,
            .pMultisampleState   = &multisample,
            .pDepthStencilState  = &uidepthstencil,
            .pColorBlendState    = &uicolorblend,
            .pDynamicState       = &uidynamics,
            .layout              = mUIPipelineLayout,
            .renderPass          = mRenderPass,
            .subpass             = kSubpassUI,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1,
        },
        VkGraphicsPipelineCreateInfo
        {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stageCount          = trianglestages.size(),
            .pStages             = trianglestages.data(),
            .pVertexInputState   = &scenevertexinput,
            .pInputAssemblyState = &assembly,
            .pTessellationState  = nullptr,
            .pViewportState      = &sceneviewport,
            .pRasterizationState = &rasterization,
            .pMultisampleState   = &multisample,
            .pDepthStencilState  = nullptr,
            // .pDepthStencilState  = &scenedepthstencil,
            .pColorBlendState    = &scenecolorblend,
            .pDynamicState       = nullptr,
            .layout              = mScenePipelineLayout,
            .renderPass          = mRenderPass,
            .subpass             = kSubpassScene,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1,
        },
    };

    CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, infos.size(), infos.data(), nullptr, mPipelines.data()));

    vkDestroyShaderModule(mDevice, shader_ui, nullptr);
    vkDestroyShaderModule(mDevice, shader_triangle, nullptr);
}

void DearImGuiShowcase::bind_resources()
{
    {// Images
        mFont.offset = 0;
        mDepth.offset = 0;
        if (mFont.memory_chunk_index == mDepth.memory_chunk_index)
            mDepth.offset = mFont.requirements.size;
        const std::array<VkBindImageMemoryInfo, 2> bindings{
            VkBindImageMemoryInfo{
                .sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                .pNext        = nullptr,
                .image        = mFont.image,
                .memory       = mMemoryChunks.at(mFont.memory_chunk_index).memory,
                .memoryOffset = mFont.offset,
            },
            VkBindImageMemoryInfo{
                .sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                .pNext        = nullptr,
                .image        = mDepth.image,
                .memory       = mMemoryChunks.at(mDepth.memory_chunk_index).memory,
                .memoryOffset = mDepth.offset,
            },
        };
        CHECK(vkBindImageMemory2(mDevice, bindings.size(), bindings.data()));
        mMemoryChunks.at(mFont.memory_chunk_index ).free -= mFont.requirements.size;
        mMemoryChunks.at(mDepth.memory_chunk_index).free -= mDepth.requirements.size;
    }
    {// Buffers
        mVertexBuffer.offset = 0;
        mIndexBuffer .offset = 0;
        mStaging     .offset = 0;
        // TODO(andrea.machizaud) we leave a gap for vertex buffer to be filled
        if (mVertexBuffer.memory_chunk_index == mIndexBuffer.memory_chunk_index)
            mIndexBuffer.offset = mVertexBuffer.offset + kInitialVertexBufferSize;
        if (mIndexBuffer.memory_chunk_index == mStaging.memory_chunk_index)
            mStaging.offset = mIndexBuffer.offset + kInitialIndexBufferSize;
        const std::array<VkBindBufferMemoryInfo, 3> bindings{
            VkBindBufferMemoryInfo{
                .sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
                .pNext        = nullptr,
                .buffer       = mVertexBuffer.buffer,
                .memory       = mMemoryChunks.at(mVertexBuffer.memory_chunk_index).memory,
                .memoryOffset = mVertexBuffer.offset,
            },
            VkBindBufferMemoryInfo{
                .sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
                .pNext        = nullptr,
                .buffer       = mIndexBuffer.buffer,
                .memory       = mMemoryChunks.at(mIndexBuffer.memory_chunk_index).memory,
                .memoryOffset = mIndexBuffer.offset,
            },
            VkBindBufferMemoryInfo{
                .sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
                .pNext        = nullptr,
                .buffer       = mStaging.buffer,
                .memory       = mMemoryChunks.at(mStaging.memory_chunk_index).memory,
                .memoryOffset = mStaging.offset,
            },
        };
        CHECK(vkBindBufferMemory2(mDevice, bindings.size(), bindings.data()));
        mMemoryChunks.at(mVertexBuffer.memory_chunk_index).free -= mVertexBuffer.requirements.size;
        mMemoryChunks.at(mIndexBuffer.memory_chunk_index ).free -= mIndexBuffer.requirements.size;
        mMemoryChunks.at(mStaging.memory_chunk_index     ).free -= mStaging.requirements.size;
    }
}

void DearImGuiShowcase::update_descriptorset()
{
    const VkDescriptorImageInfo info{
        .sampler     = VK_NULL_HANDLE,
        .imageView   = mFont.view,
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

            assert(vertexsize <= mVertexBuffer.requirements.size);
            assert(indexsize  <= mIndexBuffer .requirements.size);

            ImDrawVert* address_vertex = nullptr;
            ImDrawIdx*  address_index = nullptr;
            if ( mVertexBuffer.memory_chunk_index != mIndexBuffer.memory_chunk_index)
            {
                CHECK(vkMapMemory(
                    mDevice,
                    mMemoryChunks.at(mIndexBuffer.memory_chunk_index).memory,
                    mIndexBuffer.offset,
                    vertexsize,
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
            }
            else
            {
                {// Vertex
                    CHECK(vkMapMemory(
                        mDevice,
                        mMemoryChunks.at(mVertexBuffer.memory_chunk_index).memory,
                        mVertexBuffer.offset,
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
                    vkUnmapMemory(mDevice, mMemoryChunks.at(mVertexBuffer.memory_chunk_index).memory);
                }
                {// Index
                    CHECK(vkMapMemory(
                        mDevice,
                        mMemoryChunks.at(mIndexBuffer.memory_chunk_index).memory,
                        mIndexBuffer.offset,
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
                    vkUnmapMemory(mDevice, mMemoryChunks.at(mIndexBuffer.memory_chunk_index).memory);
                }
            }
            mVertexBuffer.occupied = vertexsize;
            mIndexBuffer .occupied = indexsize;
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
        constexpr const VkCommandBufferBeginInfo info{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .pInheritanceInfo = nullptr,
        };
        CHECK(vkBeginCommandBuffer(cmdbuffer, &info));
    }
    {// Render Passes
        { // Subpass 0 ; ImGui
            #if 0
            constexpr const std::array kClearValues{
                VkClearValue {
                    .color = VkClearColorValue{
                        .float32 = { 0.2f, 0.2f, 0.2f, 1.0f }
                    },
                },
                VkClearValue {
                    .depthStencil = VkClearDepthStencilValue{
                        .depth = 0.0,
                        .stencil = 0
                    },
                }
            };
            #else
            VkClearColorValue kClearValuesColor;
            VkClearDepthStencilValue kClearValuesDepthStencil;
            kClearValuesColor.float32[0] = 0.2f;
            kClearValuesColor.float32[1] = 0.2f;
            kClearValuesColor.float32[2] = 0.2f;
            kClearValuesColor.float32[3] = 1.0f;
            kClearValuesDepthStencil.depth   = 0.0f;
            kClearValuesDepthStencil.stencil = 0;

            const VkClearValue kClearValues[] = {
                VkClearValue {
                    .color = kClearValuesColor,
                },
                VkClearValue {
                    .depthStencil = kClearValuesDepthStencil,
                }
            };
            #endif
            #if 0
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
                .clearValueCount  = 0,
                .pClearValues     = nullptr,
            };
            #else
            VkRenderPassBeginInfo info;
            info.sType               = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            info.pNext               = nullptr;
            info.renderPass          = mRenderPass;
            info.framebuffer         = mFrameBuffers.at(presentationimage.index);
            info.renderArea.offset.x = 0;
            info.renderArea.offset.y = 0;
            info.renderArea.extent   = mResolution;
            // info.clearValueCount     = kClearValues.size();
            // info.pClearValues        = kClearValues.data();
            info.clearValueCount     = 2;
            info.pClearValues        = kClearValues;
            #endif
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
                constexpr const VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(cmdbuffer, kVertexInputBindingPosUVColor, 1, &mVertexBuffer.buffer, &offset);
                vkCmdBindIndexBuffer(cmdbuffer, mIndexBuffer.buffer, offset, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
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

                    ImGui::TextUnformatted(mProperties.deviceName);

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
            constexpr const VkPipelineStageFlags kFixedWaitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
