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
#include <numeric>
#include <iterator>

#include <array>
#include <vector>
#include <variant>
#include <unordered_map>

namespace
{
    constexpr bool kVSync = true;

    constexpr std::uint32_t kSubpassUI = 0;
    constexpr std::uint32_t kSubpassScene = 1;

    constexpr std::uint32_t kAttachmentColor = 0;
    constexpr std::uint32_t kAttachmentDepth = 1;


    constexpr std::uint32_t kVertexInputBindingPosUVColor = 0;
    constexpr std::uint32_t kUIShaderLocationPos   = 0;
    constexpr std::uint32_t kUIShaderLocationUV    = 1;
    constexpr std::uint32_t kUIShaderLocationColor = 2;

    constexpr std::uint32_t kShaderBindingFontTexture = 0;

    // TODO(andrea.machizaud) use literals...
    // TODO(andrea.machizaud) pre-allocate a reasonable amount for buffers
    constexpr std::size_t kInitialVertexBufferSize = 2 << 20; // 1 Mb
    constexpr std::size_t kInitialIndexBufferSize  = 2 << 20; // 1 Mb

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
    , mVertexBuffer(kInitialVertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
    , mIndexBuffer(kInitialIndexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
    , mStagingBuffer(kInitialIndexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
{
    {// Queue Family
        EnumerateIterator begin(std::begin(mPhysicalDevice.mQueueFamiliesProperties));
        EnumerateIterator end(std::end(mPhysicalDevice.mQueueFamiliesProperties));

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
        constexpr VkPhysicalDeviceFeatures features{
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
        constexpr VkPhysicalDeviceVulkan12Features vk12features{
            .sType                                              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext                                              = nullptr,
            .samplerMirrorClampToEdge                           = VK_FALSE,
            .drawIndirectCount                                  = VK_FALSE,
            .storageBuffer8BitAccess                            = VK_FALSE,
            .uniformAndStorageBuffer8BitAccess                  = VK_FALSE,
            .storagePushConstant8                               = VK_FALSE,
            .shaderBufferInt64Atomics                           = VK_FALSE,
            .shaderSharedInt64Atomics                           = VK_FALSE,
            .shaderFloat16                                      = VK_FALSE,
            .shaderInt8                                         = VK_FALSE,
            .descriptorIndexing                                 = VK_FALSE,
            .shaderInputAttachmentArrayDynamicIndexing          = VK_FALSE,
            .shaderUniformTexelBufferArrayDynamicIndexing       = VK_FALSE,
            .shaderStorageTexelBufferArrayDynamicIndexing       = VK_FALSE,
            .shaderUniformBufferArrayNonUniformIndexing         = VK_FALSE,
            .shaderSampledImageArrayNonUniformIndexing          = VK_FALSE,
            .shaderStorageBufferArrayNonUniformIndexing         = VK_FALSE,
            .shaderStorageImageArrayNonUniformIndexing          = VK_FALSE,
            .shaderInputAttachmentArrayNonUniformIndexing       = VK_FALSE,
            .shaderUniformTexelBufferArrayNonUniformIndexing    = VK_FALSE,
            .shaderStorageTexelBufferArrayNonUniformIndexing    = VK_FALSE,
            .descriptorBindingUniformBufferUpdateAfterBind      = VK_FALSE,
            .descriptorBindingSampledImageUpdateAfterBind       = VK_FALSE,
            .descriptorBindingStorageImageUpdateAfterBind       = VK_FALSE,
            .descriptorBindingStorageBufferUpdateAfterBind      = VK_FALSE,
            .descriptorBindingUniformTexelBufferUpdateAfterBind = VK_FALSE,
            .descriptorBindingStorageTexelBufferUpdateAfterBind = VK_FALSE,
            .descriptorBindingUpdateUnusedWhilePending          = VK_FALSE,
            .descriptorBindingPartiallyBound                    = VK_FALSE,
            .descriptorBindingVariableDescriptorCount           = VK_FALSE,
            .runtimeDescriptorArray                             = VK_FALSE,
            .samplerFilterMinmax                                = VK_FALSE,
            .scalarBlockLayout                                  = VK_FALSE,
            .imagelessFramebuffer                               = VK_FALSE,
            .uniformBufferStandardLayout                        = VK_FALSE,
            .shaderSubgroupExtendedTypes                        = VK_FALSE,
            .separateDepthStencilLayouts                        = VK_TRUE,
            .hostQueryReset                                     = VK_FALSE,
            .timelineSemaphore                                  = VK_FALSE,
            .bufferDeviceAddress                                = VK_FALSE,
            .bufferDeviceAddressCaptureReplay                   = VK_FALSE,
            .bufferDeviceAddressMultiDevice                     = VK_FALSE,
            .vulkanMemoryModel                                  = VK_FALSE,
            .vulkanMemoryModelDeviceScope                       = VK_FALSE,
            .vulkanMemoryModelAvailabilityVisibilityChains      = VK_FALSE,
            .shaderOutputViewportIndex                          = VK_FALSE,
            .shaderOutputLayer                                  = VK_FALSE,
        };
        constexpr std::array kEnabledExtensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };
        constexpr float kQueuePriorities = 1.0f;
        const VkDeviceQueueCreateInfo info_queue = VkDeviceQueueCreateInfo{
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .queueFamilyIndex = mQueueFamily,
            .queueCount       = 1,
            .pQueuePriorities = &kQueuePriorities,
        };
        mDevice = blk::Device(VkDeviceCreateInfo{
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = &vk12features,
            .flags                   = 0,
            .queueCreateInfoCount    = 1,
            .pQueueCreateInfos       = &info_queue,
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            .enabledExtensionCount   = kEnabledExtensions.size(),
            .ppEnabledExtensionNames = kEnabledExtensions.data(),
            .pEnabledFeatures        = &features,
        });
        mDevice.create(mPhysicalDevice);
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

            constexpr auto kPreferredPresentModes = {
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
    {// Render Pass
        // Pass 0 : Draw UI    (write depth)
        // Pass 1 : Draw Scene (read depth, write color)
        //  Depth discarded
        //  Color kept
        //  Transition to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR to optimize transition before presentation
        const std::array attachments{
            VkAttachmentDescription{
                .flags          = 0,
                .format         = mColorAttachmentFormat,
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
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
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            },
        };
        constexpr VkAttachmentReference write_color_reference{
            .attachment = kAttachmentColor,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        constexpr VkAttachmentReference write_stencil_reference{
            .attachment = kAttachmentDepth,
            .layout     = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
        };
        constexpr VkAttachmentReference read_stencil_reference{
            .attachment = kAttachmentDepth,
            .layout     = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
        };
        constexpr std::array<VkSubpassDescription, 2> subpasses{
            // Pass 0 : Draw UI    (write stencil, write color)
            VkSubpassDescription{
                .flags                   = 0,
                .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount    = 0,
                .pInputAttachments       = nullptr,
                .colorAttachmentCount    = 1,
                .pColorAttachments       = &write_color_reference,
                .pResolveAttachments     = nullptr,
                .pDepthStencilAttachment = &write_stencil_reference,
                .preserveAttachmentCount = 0,
                .pPreserveAttachments    = nullptr,
            },
            // Pass 1 : Draw Scene (read stencil, write color)
            VkSubpassDescription{
                .flags                   = 0,
                .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount    = 0,
                .pInputAttachments       = nullptr,
                .colorAttachmentCount    = 1,
                .pColorAttachments       = &write_color_reference,
                .pResolveAttachments     = nullptr,
                .pDepthStencilAttachment = &read_stencil_reference,
                .preserveAttachmentCount = 0,
                .pPreserveAttachments    = nullptr,
            },
        };
        constexpr std::array<VkSubpassDependency, 1> dependencies{
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

            constexpr std::array<VkPushConstantRange, 1> ranges{
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
            constexpr std::uint32_t kMaxAllocatedSets = 1;
            constexpr std::array<VkDescriptorPoolSize, 1> pool_sizes{
                // 1 sampler : font texture
                VkDescriptorPoolSize{
                    .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                }
            };
            constexpr VkDescriptorPoolCreateInfo info{
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
        {// Depth
            mDepthImage = blk::Image(
                VkExtent3D{ .width = mResolution.width, .height = mResolution.height, .depth = 1 },
                VK_IMAGE_TYPE_2D,
                mDepthStencilAttachmentFormat,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED
            );
            mDepthImage.create(mDevice);
        }
    }
    {// Buffers
        mVertexBuffer.create(mDevice);
        mIndexBuffer.create(mDevice);
        mStagingBuffer.create(mDevice);
    }
}

void DearImGuiShowcase::initialize_views()
{
    mFontImageView = blk::ImageView(
        mFontImage,
        VK_IMAGE_VIEW_TYPE_2D,
        VK_FORMAT_R8_UNORM,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    mFontImageView.create(mDevice);
    mDepthImageView = blk::ImageView(
        mDepthImage,
        VK_IMAGE_VIEW_TYPE_2D,
        mDepthStencilAttachmentFormat,
        mDepthStencilAttachmentFormat >= VK_FORMAT_D16_UNORM_S8_UINT
            ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
            : VK_IMAGE_ASPECT_DEPTH_BIT
    );
    mDepthImageView.create(mDevice);
}

void DearImGuiShowcase::allocate_memory_and_bind_resources()
{
    auto memory_type_font = mPhysicalDevice.mMemories.find_compatible(mFontImage);
    // NOTE(andrea.machizaud) not present on Windows laptop with my NVIDIA card...
    auto memory_type_depth = mPhysicalDevice.mMemories.find_compatible(mDepthImage/*, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT*/);
    auto memory_type_index = mPhysicalDevice.mMemories.find_compatible(mIndexBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
    auto memory_type_vertex = mPhysicalDevice.mMemories.find_compatible(mVertexBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
    auto memory_type_staging = mPhysicalDevice.mMemories.find_compatible(mStagingBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
    assert(memory_type_font);
    assert(memory_type_depth);
    assert(memory_type_index);
    assert(memory_type_vertex);
    assert(memory_type_staging);

    struct Resources
    {
        std::vector<blk::Image*> images;
        std::vector<blk::Buffer*> buffers;
    };

    // group resource by memory type
    std::unordered_map<const blk::MemoryType*, Resources> resources_by_types;
    // auto types = { memory_type_font, memory_type_depth, memory_type_index, memory_type_vertex, memory_type_staging};
    // auto resources = { Resource(&mFontImage),  Resource(&mDepthImage),  Resource(&mVertexBuffer),  Resource(&mIndexBuffer),  Resource(&mStagingBuffer) };
    for (auto&& [memory_type, resource] : zip(std::initializer_list<const blk::MemoryType*>{memory_type_font, memory_type_depth}, std::initializer_list<blk::Image*>{&mFontImage, &mDepthImage}))
    {
        resources_by_types[memory_type].images.push_back(resource);
    }
    for (auto&& [memory_type, resource] : zip(std::initializer_list<const blk::MemoryType*>{memory_type_index, memory_type_vertex, memory_type_staging}, std::initializer_list<blk::Buffer*>{&mVertexBuffer, &mIndexBuffer, &mStagingBuffer}))
    {
        resources_by_types[memory_type].buffers.push_back(resource);
    }

    mMemoryChunks.reserve(resources_by_types.size());
    for(auto&& entry : resources_by_types)
    {
        const VkDeviceSize required_size_images = std::accumulate(
            std::begin(entry.second.images), std::end(entry.second.images),
            VkDeviceSize{0},
            [](VkDeviceSize size, const blk::Image* image) -> VkDeviceSize{
                return size + image->mRequirements.size;
            }
        );
        const VkDeviceSize required_size_buffers = std::accumulate(
            std::begin(entry.second.buffers), std::end(entry.second.buffers),
            VkDeviceSize{0},
            [](VkDeviceSize size, const blk::Buffer* buffer) -> VkDeviceSize{
                return size + buffer->mRequirements.size;
            }
        );

        blk::Memory& chunk = mMemoryChunks.emplace_back(*entry.first, required_size_images + required_size_buffers);
        chunk.allocate(mDevice);
        if (!entry.second.images.empty())
            chunk.bind(entry.second.images);
        if (!entry.second.buffers.empty())
            chunk.bind(entry.second.buffers);
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
            *mStagingBuffer.mMemory,
            mStagingBuffer.mOffset,
            mStagingBuffer.mRequirements.size,
            0,
            reinterpret_cast<void**>(&mapped_address)
        ));

        std::copy_n(data, width * height, mapped_address);
        vkUnmapMemory(mDevice, *mStagingBuffer.mMemory);
        // Occupiped to 0 once submit completed
        mStagingBuffer.mOccupied = width * height * sizeof(unsigned char);
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

        constexpr VkCompositeAlphaFlagBitsKHR kPreferredCompositeAlphaFlags[] = {
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

        for (auto&& [image, view] : zip(mPresentation.images, mPresentation.views))
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
    for (auto&& [vkview, vkframebuffer] : zip(mPresentation.views, mFrameBuffers))
    {
        const std::array<VkImageView, 2> attachments{
            vkview,
            // NOTE(andrea.machizaud) Is it safe to re-use depth here ?
            mDepthImageView
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
        constexpr VkShaderModuleCreateInfo info{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = kShaderUI.size() * sizeof(std::uint32_t),
            .pCode    = kShaderUI.data(),
        };
        CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &shader_ui));
    }
    {// Shader - Triangle
        constexpr VkShaderModuleCreateInfo info{
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

    constexpr std::array<VkVertexInputBindingDescription, 1> vertex_bindings{
        VkVertexInputBindingDescription
        {
            .binding   = kVertexInputBindingPosUVColor,
            .stride    = sizeof(ImDrawVert),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        }
    };

    constexpr std::array<VkVertexInputAttributeDescription, 3> vertex_attributes{
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

    constexpr VkPipelineVertexInputStateCreateInfo uivertexinput{
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext                           = nullptr,
        .flags                           = 0,
        .vertexBindingDescriptionCount   = vertex_bindings.size(),
        .pVertexBindingDescriptions      = vertex_bindings.data(),
        .vertexAttributeDescriptionCount = vertex_attributes.size(),
        .pVertexAttributeDescriptions    = vertex_attributes.data(),
    };

    constexpr VkPipelineVertexInputStateCreateInfo scenevertexinput{
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

    constexpr VkPipelineViewportStateCreateInfo uiviewport{
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

    constexpr VkPipelineDepthStencilStateCreateInfo uidepthstencil{
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .depthTestEnable       = VK_FALSE,
        .depthWriteEnable      = VK_FALSE,
        // .depthCompareOp        = VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_TRUE,
        .front                 = VkStencilOpState{
            .failOp      = VK_STENCIL_OP_KEEP,
            .passOp      = VK_STENCIL_OP_REPLACE,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp   = VK_COMPARE_OP_ALWAYS,
            .compareMask = 0xFF,
            .writeMask   = 0xFF,
            .reference   = 0x01,
        },
        .back                  = VkStencilOpState{
            .failOp      = VK_STENCIL_OP_KEEP,
            .passOp      = VK_STENCIL_OP_REPLACE,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp   = VK_COMPARE_OP_ALWAYS,
            .compareMask = 0xFF,
            .writeMask   = 0xFF,
            .reference   = 0x01,
        },
    };

    constexpr VkPipelineDepthStencilStateCreateInfo scenedepthstencil{
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext                 = nullptr,
        .flags                 = 0,
        .depthTestEnable       = VK_FALSE,
        .depthWriteEnable      = VK_FALSE,
        // .depthCompareOp        = VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_TRUE,
        .front                 = VkStencilOpState{
            .failOp      = VK_STENCIL_OP_KEEP,
            .passOp      = VK_STENCIL_OP_KEEP,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp   = VK_COMPARE_OP_NOT_EQUAL,
            .compareMask = 0xFF,
            .reference   = 0x01,
        },
        .back                  = VkStencilOpState{
            .failOp      = VK_STENCIL_OP_KEEP,
            .passOp      = VK_STENCIL_OP_KEEP,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp   = VK_COMPARE_OP_NOT_EQUAL,
            .compareMask = 0xFF,
            .reference   = 0x01,
        },
    };

    constexpr std::array<VkPipelineColorBlendAttachmentState, 1> colorblendattachments_passthrough{
        VkPipelineColorBlendAttachmentState{
            .blendEnable         = VK_FALSE,
            .colorWriteMask      =
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT,
        }
    };

    constexpr std::array<VkPipelineColorBlendAttachmentState, 1> colorblendattachments_blend{
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

    constexpr VkPipelineColorBlendStateCreateInfo uicolorblend{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .logicOpEnable           = VK_FALSE,
        .logicOp                 = VK_LOGIC_OP_CLEAR,
        .attachmentCount         = colorblendattachments_passthrough.size(),
        .pAttachments            = colorblendattachments_passthrough.data(),
    };

    constexpr VkPipelineColorBlendStateCreateInfo scenecolorblend{
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext                   = nullptr,
        .flags                   = 0,
        .logicOpEnable           = VK_FALSE,
        .logicOp                 = VK_LOGIC_OP_COPY,
        .attachmentCount         = colorblendattachments_blend.size(),
        .pAttachments            = colorblendattachments_blend.data(),
        .blendConstants          = { 0.0f, 0.0f, 0.0f, 0.0f },
    };

    constexpr std::array<VkDynamicState, 2> states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    constexpr VkPipelineDynamicStateCreateInfo uidynamics{
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
            .pDepthStencilState  = &scenedepthstencil,
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

void DearImGuiShowcase::update_descriptorset()
{
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
            #if 0
            // constexpr VkClearValue kClearValues[2] = {
            constexpr std::array kClearValues {
            // constexpr std::array<VkClearValue, 2> kClearValues = std::to_array(VkClearValue[2]{
                VkClearValue {
                    .color = VkClearColorValue{
                        .float32 = { 0.2f, 0.2f, 0.2f, 1.0f }
                    },
                    .depthStencil = VkClearDepthStencilValue{
                    }
                },
                VkClearValue {
                    .color = VkClearColorValue{
                    },
                    .depthStencil = VkClearDepthStencilValue{
                        .depth   = 0.0f,
                        .stencil = 0,
                    }
                }
            };
            // });
            #else
            VkClearColorValue kClearValuesColor;
            kClearValuesColor.float32[0] = 0.2f;
            kClearValuesColor.float32[1] = 0.2f;
            kClearValuesColor.float32[2] = 0.2f;
            kClearValuesColor.float32[3] = 1.0f;
            VkClearDepthStencilValue kClearValuesDepthStencil;
            kClearValuesDepthStencil.depth   = 0.0f;
            kClearValuesDepthStencil.stencil = 0;

            const std::array kClearValues{
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
            info.clearValueCount     = kClearValues.size();
            info.pClearValues        = kClearValues.data();
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
