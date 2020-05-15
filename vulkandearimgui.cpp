#include <vulkan/vulkan.h>

#include <imgui.h>

#include <algorithm>
#include <type_traits>

#include "./ui.vertex.hpp"
#include "./ui.fragment.hpp"

#include "./font.hpp"

#include "./vulkandebug.hpp"

#include "./vulkanbuffer.hpp"
#include "./vulkanscopedbuffermapping.hpp"

#include "./vulkandearimgui.hpp"

namespace
{
    struct Constants {
        float scale    [2];
        float translate[2];
    };
}

VulkanDearImGui::VulkanDearImGui(
        const VkInstance& instance,
        const VkPhysicalDevice& physical_device,
        const VkDevice& device)
    : VulkanPhysicalDeviceBase(physical_device)
    , mInstance(instance)
    , mPhysicalDevice(physical_device)
    , mDevice(device)
    , mContext(ImGui::CreateContext())
    , mVertex{
        .count = 0u,
        .buffer = VulkanBuffer(
            mDevice,
            mMemoryProperties,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        )
    }
    , mIndex{
        .count = 0u,
        .buffer = VulkanBuffer(
            mDevice,
            mMemoryProperties,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        )
    }
{
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
        io.BackendRendererName = "vktheblackunknown";
    }
}

VulkanDearImGui::~VulkanDearImGui()
{
    vkDestroyPipeline(mDevice, mPipeline, nullptr);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);

    vkDestroyShaderModule(mDevice, mShaderModuleUIFragment, nullptr);
    vkDestroyShaderModule(mDevice, mShaderModuleUIVertex, nullptr);

    vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

    for (VkFramebuffer framebuffer : mFrameBuffers)
    {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }

    vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

    vkDestroySampler(mDevice, mFont.sampler, nullptr);
    vkDestroyImageView(mDevice, mFont.view, nullptr);
    vkFreeMemory(mDevice, mFont.memory, nullptr);
    vkDestroyImage(mDevice, mFont.image, nullptr);

    vkDestroyImageView(mDevice, mDepth.view, nullptr);
    vkDestroyImage(mDevice, mDepth.image, nullptr);
    vkFreeMemory(mDevice, mDepth.memory, nullptr);

    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

    ImGui::DestroyContext(mContext);
}

void VulkanDearImGui::setup_depth(const VkExtent2D& dimension, VkFormat depth_format)
{
    mDepth.format = depth_format;
    {// Image
        const VkImageCreateInfo info_image{
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = depth_format,
            .extent                = VkExtent3D{
                .width  = dimension.width,
                .height = dimension.height,
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
        CHECK(vkCreateImage(mDevice, &info_image, nullptr, &mDepth.image));
    }
    {// Memory
        VkMemoryRequirements requirements;
        vkGetImageMemoryRequirements(mDevice, mDepth.image, &requirements);

        auto memory_type_index = get_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        assert(memory_type_index.has_value());

        const VkMemoryAllocateInfo info_allocation{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = requirements.size,
            .memoryTypeIndex = memory_type_index.value(),
        };
        CHECK(vkAllocateMemory(mDevice, &info_allocation, nullptr, &mDepth.memory));
        CHECK(vkBindImageMemory(mDevice, mDepth.image, mDepth.memory, 0u));
    }
    {// View
        VkImageViewCreateInfo info_view{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .image            = mDepth.image,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = depth_format,
            .components       = VkComponentMapping{
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = VkImageSubresourceRange{
                .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        if(depth_format >= VK_FORMAT_D16_UNORM_S8_UINT)
            info_view.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        CHECK(vkCreateImageView(mDevice, &info_view, nullptr, &mDepth.view));
    }
}

void VulkanDearImGui::setup_font()
{
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

        int width = 0, height = 0;
        unsigned char* data = nullptr;
        io.Fonts->GetTexDataAsAlpha8(&data, &width, &height);

        VkDeviceSize size = width * height * sizeof(unsigned char);

        {// Image
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
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
                .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED,
            };
            CHECK(vkCreateImage(mDevice, &info, nullptr, &mFont.image));
        }
        {// Memory
            VkMemoryRequirements requirements;
            vkGetImageMemoryRequirements(mDevice, mFont.image, &requirements);

            auto memory_type_index = get_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            assert(memory_type_index.has_value());

            const VkMemoryAllocateInfo info{
                .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext           = nullptr,
                .allocationSize  = requirements.size,
                .memoryTypeIndex = memory_type_index.value(),
            };
            CHECK(vkAllocateMemory(mDevice, &info, nullptr, &mFont.memory));
            CHECK(vkBindImageMemory(mDevice, mFont.image, mFont.memory, 0u));
        }
        {// View
            VkImageViewCreateInfo info{
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
        {// Upload
            VulkanBuffer staging(
                mDevice,
                mMemoryProperties,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            staging.allocate(size);

            {// Memory Mapping
                ScopedBufferMapping<std::remove_pointer_t<decltype(data)>> mapping(staging);
                std::copy(data, std::next(data, size), mapping.mMappedMemory);

                const VkMappedMemoryRange range{
                    .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                    .pNext  = nullptr,
                    .memory = staging.mMemory,
                    .offset = 0,
                    .size   = VK_WHOLE_SIZE
                };
                CHECK(vkFlushMappedMemoryRanges(mDevice, 1, &range));
            }
            {// Transfer / Copy / Shader Read
                VkCommandBuffer command_buffer = create_command_buffer(mCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

                const VkCommandBufferBeginInfo info{
                    .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .pNext            = nullptr,
                    .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                    .pInheritanceInfo = nullptr,
                };
                CHECK(vkBeginCommandBuffer(command_buffer, &info));

                const VkImageMemoryBarrier barrier_transfer{
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
                    command_buffer,
                    VK_PIPELINE_STAGE_HOST_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier_transfer);

                const VkBufferImageCopy copy_region{
                    .bufferOffset      = 0,
                    .bufferRowLength   = 0,
                    .bufferImageHeight = 0,
                    .imageSubresource  = VkImageSubresourceLayers{
                        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel       = 0,
                        .baseArrayLayer = 0,
                        .layerCount     = 1,
                    },
                    .imageOffset       = VkOffset3D{
                        .x = 0,
                        .y = 0,
                        .z = 0,
                    },
                    .imageExtent       = VkExtent3D{
                        .width  = static_cast<std::uint32_t>(width),
                        .height = static_cast<std::uint32_t>(height),
                        .depth  = 1,
                    },
                };

                vkCmdCopyBufferToImage(
                    command_buffer,
                    staging.mBuffer,
                    mFont.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &copy_region
                );

                const VkImageMemoryBarrier barrier_shader_read{
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
                    },
                };
                vkCmdPipelineBarrier(
                    command_buffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier_shader_read);

                CHECK(vkEndCommandBuffer(command_buffer));

                submit_command_buffer(mQueue, command_buffer);

                destroy_command_buffer(mCommandPool, command_buffer);
            }
        }
    }
    {// Sampler
        const VkSamplerCreateInfo info{
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
        CHECK(vkCreateSampler(mDevice, &info, nullptr, &mFont.sampler));
    }
    {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->TexID = reinterpret_cast<ImTextureID>(mFont.image);
    }
    // TODO ImFontAtlas::SetTexID
}

void VulkanDearImGui::setup_queues(std::uint32_t family_index)
{
    vkGetDeviceQueue(mDevice, family_index, 0, &mQueue);
    {// Command Pool
        VkCommandPoolCreateInfo info_pool{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = family_index,
        };
        CHECK(vkCreateCommandPool(mDevice, &info_pool, nullptr, &mCommandPool));
    }
}

void VulkanDearImGui::setup_descriptors()
{
    {// Descriptor
        const VkDescriptorPoolSize size{
            .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
        };
        const VkDescriptorPoolCreateInfo info{
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = 0,
            .maxSets       = 2,
            .poolSizeCount = 1,
            .pPoolSizes    = &size,
        };
        CHECK(vkCreateDescriptorPool(mDevice, &info, nullptr, &mDescriptorPool));
    }
    {// Descriptor Layout
        const VkDescriptorSetLayoutBinding binding{
            .binding            = 0,
            .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount    = 1,
            .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = &mFont.sampler,
        };
        const VkDescriptorSetLayoutCreateInfo info{
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = 0,
            .bindingCount  = 1,
            .pBindings     = &binding,
        };
        CHECK(vkCreateDescriptorSetLayout(mDevice, &info, nullptr, &mDescriptorSetLayout));
    }
    {// Descriptor Set
        const VkDescriptorSetAllocateInfo info{
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorPool     = mDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &mDescriptorSetLayout,
        };
        CHECK(vkAllocateDescriptorSets(mDevice, &info, &mDescriptorSet));
    }
    {// Descriptor Set Update
        const VkDescriptorImageInfo info{
            .sampler     = mFont.sampler,
            .imageView   = mFont.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        const VkWriteDescriptorSet write{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = mDescriptorSet,
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorCount  = 1,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = &info,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr,
        };
        vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);
    }
}

void VulkanDearImGui::setup_shaders()
{
    {// Shader Modules - UI Vertex
        const VkShaderModuleCreateInfo info{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = sizeof(shader_ui_vertex),
            .pCode    = shader_ui_vertex,
        };
        CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &mShaderModuleUIVertex));
    }
    {// Shader Modules - UI Fragment
        const VkShaderModuleCreateInfo info{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = sizeof(shader_ui_fragment),
            .pCode    = shader_ui_fragment,
        };
        CHECK(vkCreateShaderModule(mDevice, &info, nullptr, &mShaderModuleUIFragment));
    }
}

void VulkanDearImGui::setup_render_pass(VkFormat color_format, VkFormat depth_format)
{
    const VkAttachmentReference reference_color{
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    const VkAttachmentReference reference_depth{
        .attachment = 1,
        .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    const VkSubpassDescription subpass{
        .flags                   = 0,
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount    = 0,
        .pInputAttachments       = nullptr,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &reference_color,
        .pResolveAttachments     = nullptr,
        .pDepthStencilAttachment = &reference_depth,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments    = nullptr,
    };
    const VkSubpassDependency dependencies[2] = {
        VkSubpassDependency{
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            // .srcStageMask needs to be a part of pWaitDstStageMask in the WSI semaphore.
            .srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
        VkSubpassDependency{
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
    };
    const VkAttachmentDescription attachments[2] = {
        VkAttachmentDescription{
            .flags          = 0,
            .format         = color_format,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            // The image will automatically be transitioned from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL for rendering, then out to PRESENT_SRC_KHR at the end.
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            // Presenting images in Vulkan requires a special layout.
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
        VkAttachmentDescription{
            .flags          = 0,
            .format         = depth_format,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        },
    };
    const VkRenderPassCreateInfo info_renderpass{
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0,
        .attachmentCount = sizeof(attachments) / sizeof(attachments[0]),
        .pAttachments    = attachments,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = sizeof(dependencies) / sizeof(dependencies[0]),
        .pDependencies   = dependencies,
    };
    CHECK(vkCreateRenderPass(mDevice, &info_renderpass, nullptr, &mRenderPass));
}

void VulkanDearImGui::setup_framebuffers(const VkExtent2D& dimension, const std::vector<VkImageView>& views)
{
    assert(mRenderPass != VK_NULL_HANDLE);
    assert(mDepth.view != VK_NULL_HANDLE);

    VkImageView attachments[2];

    // Same depth/stencil for all framebuffer
    attachments[1] = mDepth.view;

    const VkFramebufferCreateInfo info{
        .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext           = nullptr,
        .flags           = 0u,
        .renderPass      = mRenderPass,
        .attachmentCount = 2,
        .pAttachments    = attachments,
        .width           = dimension.width,
        .height          = dimension.height,
        .layers          = 1,
    };
    mFrameBuffers.resize(views.size());

    for (std::uint32_t idx = 0u, count = mFrameBuffers.size(); idx < count; ++idx)
    {
        attachments[0] = views.at(idx);
        CHECK(vkCreateFramebuffer(mDevice, &info, nullptr, &mFrameBuffers.at(idx)));
    }
}

void VulkanDearImGui::setup_graphics_pipeline(const VkExtent2D& dimension)
{
    assert(mRenderPass != VK_NULL_HANDLE);
    {// IO
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(dimension.width, dimension.height);
    }
    {// Pipeline Cache
        VkPipelineCacheCreateInfo info{
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0u,
            // TODO
            .initialDataSize = 0u,
            .pInitialData    = nullptr,
        };
        CHECK(vkCreatePipelineCache(mDevice, &info, nullptr, &mPipelineCache));
    }
    {// Pipeline Layout
        const VkPushConstantRange range{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset     = 0,
            .size       = sizeof(Constants),
        };
        const VkPipelineLayoutCreateInfo info{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = 1,
            .pSetLayouts            = &mDescriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges    = &range,
        };
        CHECK(vkCreatePipelineLayout(mDevice, &info, nullptr, &mPipelineLayout));
    }
    {// Pipeline
        const VkPipelineShaderStageCreateInfo stages[] = {
            VkPipelineShaderStageCreateInfo{
                .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext               = nullptr,
                .flags               = 0,
                .stage               = VK_SHADER_STAGE_VERTEX_BIT,
                .module              = mShaderModuleUIVertex,
                .pName               = "main",
                .pSpecializationInfo = nullptr,
            },
            VkPipelineShaderStageCreateInfo{
                .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext               = nullptr,
                .flags               = 0,
                .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module              = mShaderModuleUIFragment,
                .pName               = "main",
                .pSpecializationInfo = nullptr,
            },
        };
        const VkVertexInputBindingDescription vertex_input_bindings{
            .binding   = 0u,
            .stride    = sizeof(ImDrawVert),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        const VkVertexInputAttributeDescription vertex_input_attributes[] = {
            VkVertexInputAttributeDescription{
                .location = 0u,
                .binding  = vertex_input_bindings.binding,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = offsetof(ImDrawVert, pos),
            },
            VkVertexInputAttributeDescription{
                .location = 1u,
                .binding  = vertex_input_bindings.binding,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = offsetof(ImDrawVert, uv),
            },
            VkVertexInputAttributeDescription{
                .location = 2u,
                .binding  = vertex_input_bindings.binding,
                .format   = VK_FORMAT_R8G8B8A8_UNORM,
                .offset   = offsetof(ImDrawVert, col),
            },
        };
        const VkPipelineVertexInputStateCreateInfo vertex_input_state{
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext                           = nullptr,
            .flags                           = 0,
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &vertex_input_bindings,
            .vertexAttributeDescriptionCount = sizeof(vertex_input_attributes) / sizeof(vertex_input_attributes[0]),
            .pVertexAttributeDescriptions    = vertex_input_attributes,
        };
        const VkPipelineInputAssemblyStateCreateInfo assembly{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };
        const VkPipelineViewportStateCreateInfo viewport{
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = 0,
            .viewportCount = 1,
            .pViewports    = nullptr, // dynamic state
            .scissorCount  = 1,
            .pScissors     = nullptr, // dynamic state
        };
        const VkPipelineRasterizationStateCreateInfo rasterization{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .cullMode                = VK_CULL_MODE_NONE,
            .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable         = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp          = 0.0f,
            .depthBiasSlopeFactor    = 0.0f,
            .lineWidth               = 1.0f,
        };
        const VkPipelineMultisampleStateCreateInfo multisample{
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
        const VkPipelineDepthStencilStateCreateInfo depth_stencil{
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .depthTestEnable       = VK_FALSE,
            .depthWriteEnable      = VK_FALSE,
            .depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable     = VK_FALSE,
            .front                 = VkStencilOpState{
                .failOp      = VK_STENCIL_OP_KEEP,
                .passOp      = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp   = VK_COMPARE_OP_NEVER,
                .compareMask = 0u,
                .writeMask   = 0u,
                .reference   = 0u,
            },
            .back                  = VkStencilOpState{
                .failOp      = VK_STENCIL_OP_KEEP,
                .passOp      = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp   = VK_COMPARE_OP_ALWAYS,
                .compareMask = 0u,
                .writeMask   = 0u,
                .reference   = 0u,
            },
            .minDepthBounds        = 0.0f,
            .maxDepthBounds        = 0.0f,
        };
        const VkPipelineColorBlendAttachmentState color_blend_attachment_state{
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
        };
        const VkPipelineColorBlendStateCreateInfo color_blend_state{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .logicOpEnable           = VK_FALSE,
            .logicOp                 = VK_LOGIC_OP_CLEAR,
            .attachmentCount         = 1,
            .pAttachments            = &color_blend_attachment_state,
            .blendConstants          = { 0.0f, 0.0f, 0.0f, 0.0f },
        };
        const VkDynamicState states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        const VkPipelineDynamicStateCreateInfo dynamic_state{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .dynamicStateCount       = sizeof(states) / sizeof(states[0]),
            .pDynamicStates          = states,
        };
        const VkGraphicsPipelineCreateInfo info{
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stageCount          = sizeof(stages) / sizeof(stages[0]),
            .pStages             = stages,
            .pVertexInputState   = &vertex_input_state,
            .pInputAssemblyState = &assembly,
            .pTessellationState  = nullptr,
            .pViewportState      = &viewport,
            .pRasterizationState = &rasterization,
            .pMultisampleState   = &multisample,
            .pDepthStencilState  = &depth_stencil,
            .pColorBlendState    = &color_blend_state,
            .pDynamicState       = &dynamic_state,
            .layout              = mPipelineLayout,
            .renderPass          = mRenderPass,
            .subpass             = 0,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1,
        };
        CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &info, nullptr, &mPipeline));

        // TODO Destroy shaders
    }
}
