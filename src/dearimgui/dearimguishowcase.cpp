#include "./dearimguishowcase.hpp"

#include "../utilities.hpp"
#include "../vkutilities.hpp"
#include "../vkdebug.hpp"

#include "font.hpp"
#include "ui.vertex.hpp"
#include "ui.fragment.hpp"

#include <vulkan/vulkan_core.h>

#include <imgui.h>

#include <cassert>
#include <cinttypes>
#include <cstddef>

#include <limits>

#include <iterator>
#include <vector>
#include <array>

namespace
{
    constexpr const bool kVSync = true;

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
            //ImGui::StyleColorsClassic(&style);
            //ImGui::StyleColorsLight(&style);
        }
        {// IO
            ImGuiIO& io = ImGui::GetIO();
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
            io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
            io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
            io.BackendPlatformName = "Win32";
            io.BackendRendererName = "vkplaygrounds";
        }
        {// Font
            ImGuiIO& io = ImGui::GetIO();
            {
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
    for(auto&& view : mPresentation.views)
        vkDestroyImageView(mDevice, view, nullptr);
    vkDestroySwapchainKHR(mDevice, mPresentation.swapchain, nullptr);

    vkFreeMemory(mDevice, mMemoryCPUCoherent.memory, nullptr);
    vkFreeMemory(mDevice, mMemoryGPU.memory, nullptr);

    vkDestroyBuffer(mDevice, mIndexBuffer.buffer, nullptr);
    vkDestroyBuffer(mDevice, mVertexBuffer.buffer, nullptr);

    vkDestroyImageView(mDevice, mFont.view, nullptr);
    vkDestroyImage(mDevice, mFont.image, nullptr);

    vkDestroySemaphore(mDevice, mSemaphoreRenderComplete, nullptr);
    vkDestroySemaphore(mDevice, mSemaphorePresentComplete, nullptr);

    vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);

    vkDestroyShaderModule(mDevice, mShaderModuleUIFragment, nullptr);
    vkDestroyShaderModule(mDevice, mShaderModuleUIVertex, nullptr);

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
            .magFilter               = VK_FILTER_LINEAR,
            .minFilter               = VK_FILTER_LINEAR,
            .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
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
                .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            },
        };
        constexpr const std::uint32_t kAttachmentColor = 0;
        constexpr const std::uint32_t kAttachmentDepth = 1;
        constexpr const VkAttachmentReference color_reference{
            .attachment = kAttachmentColor,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        constexpr const VkAttachmentReference depth_reference{
            .attachment = kAttachmentDepth,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        constexpr const std::uint32_t kSubpassUI = 0;
        constexpr const std::uint32_t kSubpassScene = 1;
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
                .pDepthStencilAttachment = &depth_reference,
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
    {// Shader Modules
        {// UI Vertex
            constexpr const VkShaderModuleCreateInfo info{
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext    = nullptr,
                .flags    = 0,
                .codeSize = sizeof(shader_ui_vertex),
                .pCode    = shader_ui_vertex,
            };
            CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &mShaderModuleUIVertex));
        }
        {// UI Fragment
            constexpr const VkShaderModuleCreateInfo info{
                .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext    = nullptr,
                .flags    = 0,
                .codeSize = sizeof(shader_ui_fragment),
                .pCode    = shader_ui_fragment,
            };
            CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &mShaderModuleUIFragment));
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
        CHECK(vkCreatePipelineLayout(mDevice, &info, nullptr, &mPipelineLayout));
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
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mSemaphorePresentComplete));
        CHECK(vkCreateSemaphore(mDevice, &info_semaphore, nullptr, &mSemaphoreRenderComplete));
    }
}

void DearImGuiShowcase::initialize_resources()
{
    // NVIDIA - https://developer.nvidia.com/blog/vulkan-dos-donts/
    //  > Parallelize command buffer recording, image and buffer creation, descriptor set updates, pipeline creation, and memory allocation / binding. Task graph architecture is a good option which allows sufficient parallelism in terms of draw submission while also respecting resource and command queue dependencies.
    {// Font
        ImGuiIO& io = ImGui::GetIO();
        {// Image
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
    {// Geometry
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
            mVertexBuffer.offset       = 0;
            mVertexBuffer.current_size = info.size;
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
            // TODO(andrea.machizaud) we leave a gap for vertex buffer to be filled
            mIndexBuffer.offset       = kInitialVertexBufferSize;
            mIndexBuffer.current_size = info.size;
        }
    }
}

void DearImGuiShowcase::initialize_views()
{
    {// Font
        {// View
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
    }
}

void DearImGuiShowcase::allocate_memory()
{
    {// GPU : Font (static, defined and uploaded once)
        auto memory_type_index = get_memory_type(
            mMemoryProperties, mFont.requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        assert(memory_type_index.has_value());

        const VkMemoryAllocateInfo info{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = mFont.requirements.size,
            .memoryTypeIndex = memory_type_index.value(),
        };
        CHECK(vkAllocateMemory(mDevice, &info, nullptr, &mMemoryGPU.memory));
        mMemoryGPU.free = info.allocationSize;
    }
    {// CPU : UI Geometry (quite dynamic)
        assert(mVertexBuffer.requirements.memoryTypeBits == mIndexBuffer.requirements.memoryTypeBits);

        auto memory_type_index = get_memory_type(
            mMemoryProperties, mVertexBuffer.requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        assert(memory_type_index.has_value());

        const VkMemoryAllocateInfo info{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = mVertexBuffer.requirements.size + mIndexBuffer.requirements.size,
            .memoryTypeIndex = memory_type_index.value(),
        };
        CHECK(vkAllocateMemory(mDevice, &info, nullptr, &mMemoryCPUCoherent.memory));
        mMemoryCPUCoherent.free = info.allocationSize;
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

        for (auto&& [image, view] : IteratorRange(mPresentation.images, mPresentation.views))
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

void DearImGuiShowcase::bind_resources()
{
    {// Images
        assert(mMemoryGPU.free >= mFont.requirements.size);
        const std::array<VkBindImageMemoryInfo, 1> bindings{
            VkBindImageMemoryInfo{
                .sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                .pNext        = nullptr,
                .image        = mFont.image,
                .memory       = mMemoryGPU.memory,
                .memoryOffset = 0u,
            },
        };
        CHECK(vkBindImageMemory2(mDevice, bindings.size(), bindings.data()));
        mMemoryGPU.free -= mFont.requirements.size;
    }
    {// Buffers
        assert(mMemoryCPUCoherent.free >= (mVertexBuffer.requirements.size + mIndexBuffer.requirements.size));
        const std::array<VkBindBufferMemoryInfo, 2> bindings{
            VkBindBufferMemoryInfo{
                .sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
                .pNext        = nullptr,
                .buffer       = mVertexBuffer.buffer,
                .memory       = mMemoryCPUCoherent.memory,
                .memoryOffset = mVertexBuffer.offset,
            },
            VkBindBufferMemoryInfo{
                .sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
                .pNext        = nullptr,
                .buffer       = mIndexBuffer.buffer,
                .memory       = mMemoryCPUCoherent.memory,
                .memoryOffset = mIndexBuffer.offset,
            },
        };
        CHECK(vkBindBufferMemory2(mDevice, bindings.size(), bindings.data()));
        mMemoryCPUCoherent.free -= mVertexBuffer.requirements.size;
        mMemoryCPUCoherent.free -= mIndexBuffer.requirements.size;
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