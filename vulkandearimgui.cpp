#include <vulkan/vulkan.h>

#include <imgui.h>

#include <cstring>

#include <chrono>
#include <limits>
#include <iterator>
#include <algorithm>

#include <filesystem>

#include <type_traits>

#include "./ui.vertex.hpp"
#include "./ui.fragment.hpp"

#include "./font.hpp"

#include "./vulkandebug.hpp"

#include "./vulkanbuffer.hpp"
#include "./vulkansurface.hpp"
#include "./vulkanapplication.hpp"
#include "./vulkanscopedbuffermapping.hpp"
#include "./vulkanscopedpresentableimage.hpp"

#include "./vulkandearimgui.hpp"

using namespace std::literals::chrono_literals;

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
    {// Style
        ImGuiStyle& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_TitleBg         ] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
        style.Colors[ImGuiCol_TitleBgActive   ] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);

        style.Colors[ImGuiCol_MenuBarBg       ] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);

        style.Colors[ImGuiCol_Header          ] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_HeaderActive    ] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_HeaderHovered   ] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);

        style.Colors[ImGuiCol_FrameBg         ] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_FrameBgHovered  ] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
        style.Colors[ImGuiCol_FrameBgActive   ] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);

        style.Colors[ImGuiCol_CheckMark       ] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);

        style.Colors[ImGuiCol_SliderGrab      ] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);

        style.Colors[ImGuiCol_Button          ] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        style.Colors[ImGuiCol_ButtonHovered   ] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
        style.Colors[ImGuiCol_ButtonActive    ] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    }
    {// IO
        ImGuiIO& io = ImGui::GetIO();
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
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
            .minLod                  = 0.0f,
            .maxLod                  = 0.0f,
            .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE,
        };
        CHECK(vkCreateSampler(mDevice, &info, nullptr, &mFont.sampler));
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
            // TODO ?
            .pImmutableSamplers = nullptr,
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
                .binding  = 0u,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = offsetof(ImDrawVert, pos),
            },
            VkVertexInputAttributeDescription{
                .location = 1u,
                .binding  = 0u,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = offsetof(ImDrawVert, uv),
            },
            VkVertexInputAttributeDescription{
                .location = 2u,
                .binding  = 0u,
                .format   = VK_FORMAT_R8G8B8_UNORM,
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
    }
}

void VulkanDearImGui::build_imgui_command_buffers(VulkanSurface& surface)
{
    const VkCommandBufferBeginInfo info_cmdbuffer{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = 0,
        .pInheritanceInfo = nullptr,
    };

    const VkClearValue clear_values[] = {
        VkClearValue{
            .color = VkClearColorValue{
                .float32 = { 0.2f, 0.2f, 0.2f, 1.0f }
            }
        },
        VkClearValue{
            .depthStencil = VkClearDepthStencilValue{
                .depth   = 1.0f,
                .stencil = 0u
            }
        }
    };

    VkRenderPassBeginInfo info_renderpassbegin{
        .sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext            = nullptr,
        .renderPass       = mRenderPass,
        .framebuffer      = VK_NULL_HANDLE,
        .renderArea       = VkRect2D{
            .offset = VkOffset2D{
                .x = 0,
                .y = 0,
            },
            .extent = surface.mResolution
        },
        .clearValueCount  = sizeof(clear_values) / sizeof(clear_values[0]),
        .pClearValues     = clear_values,
    };

    const VkViewport viewport{
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = static_cast<float>(surface.mResolution.width),
        .height   = static_cast<float>(surface.mResolution.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    const VkRect2D scissor{
        .offset   = VkOffset2D{
            .x = 0,
            .y = 0,
        },
        .extent   = surface.mResolution,
    };

    // TODO cf. ImGui::buildCommandBuffers

    new_frame(mBenchmark.frame_counter == 0);
    update_imgui_draw_data();

    for (std::uint32_t idx = 0u, count = surface.mCommandBuffers.size(); idx < count; ++idx)
    {
        VkFramebuffer   framebuffer = mFrameBuffers.at(idx);
        VkCommandBuffer cmdbuffer   = surface.mCommandBuffers.at(idx);

        info_renderpassbegin.framebuffer = framebuffer;

        CHECK(vkBeginCommandBuffer(cmdbuffer, &info_cmdbuffer));

        vkCmdBeginRenderPass(cmdbuffer, &info_renderpassbegin, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(cmdbuffer, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuffer, 0, 1, &scissor);

        vkCmdBindDescriptorSets(
            cmdbuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            mPipelineLayout,
            0, 1, &mDescriptorSet,
            0, nullptr
        );
        vkCmdBindPipeline(cmdbuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            mPipeline
        );

        constexpr const VkDeviceSize offset = 0;
        // TODO
        // if (mUI.background)
        // {
        //     vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &mModels.background.vertices.mBuffer, &offset);
        //     vkCmdBindIndexBuffer(cmdbuffer, &mModels.background.indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
        //     vkCmdDrawIndexed(cmdbuffer, &mModels.background.indexCount, 1, 0, 0, 0);
        // }

        // TODO
        // if (mUI.models)
        // {
        //     vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &mModels.models.vertices.mBuffer, &offset);
        //     vkCmdBindIndexBuffer(cmdbuffer, &mModels.models.indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
        //     vkCmdDrawIndexed(cmdbuffer, &mModels.models.indexCount, 1, 0, 0, 0);
        // }

        // TODO
        // if (mUI.logos)
        // {
        //     vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &mModels.logos.vertices.mBuffer, &offset);
        //     vkCmdBindIndexBuffer(cmdbuffer, &mModels.logos.indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
        //     vkCmdDrawIndexed(cmdbuffer, &mModels.logos.indexCount, 1, 0, 0, 0);
        // }

        build_imgui_command_buffer(cmdbuffer);

        vkCmdEndRenderPass(cmdbuffer);

        CHECK(vkEndCommandBuffer(cmdbuffer));
    }
}

void VulkanDearImGui::build_imgui_command_buffers(VulkanSurface& surface, std::uint32_t idx)
{
    const VkCommandBufferBeginInfo info_cmdbuffer{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext            = nullptr,
        .flags            = 0,
        .pInheritanceInfo = nullptr,
    };

    const VkClearValue clear_values[] = {
        VkClearValue{
            .color = VkClearColorValue{
                .float32 = { 0.2f, 0.2f, 0.2f, 1.0f }
            }
        },
        VkClearValue{
            .depthStencil = VkClearDepthStencilValue{
                .depth   = 1.0f,
                .stencil = 0u
            }
        }
    };

    VkRenderPassBeginInfo info_renderpassbegin{
        .sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext            = nullptr,
        .renderPass       = mRenderPass,
        .framebuffer      = VK_NULL_HANDLE,
        .renderArea       = VkRect2D{
            .offset = VkOffset2D{
                .x = 0,
                .y = 0,
            },
            .extent = surface.mResolution
        },
        .clearValueCount  = sizeof(clear_values) / sizeof(clear_values[0]),
        .pClearValues     = clear_values,
    };

    const VkViewport viewport{
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = static_cast<float>(surface.mResolution.width),
        .height   = static_cast<float>(surface.mResolution.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    const VkRect2D scissor{
        .offset   = VkOffset2D{
            .x = 0,
            .y = 0,
        },
        .extent   = surface.mResolution,
    };

    // TODO cf. ImGui::buildCommandBuffers

    new_frame(mBenchmark.frame_counter == 0);
    update_imgui_draw_data();

    {
        VkFramebuffer   framebuffer = mFrameBuffers.at(idx);
        VkCommandBuffer cmdbuffer   = surface.mCommandBuffers.at(idx);

        info_renderpassbegin.framebuffer = framebuffer;

        CHECK(vkBeginCommandBuffer(cmdbuffer, &info_cmdbuffer));

        vkCmdBeginRenderPass(cmdbuffer, &info_renderpassbegin, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(cmdbuffer, 0, 1, &viewport);
        vkCmdSetScissor(cmdbuffer, 0, 1, &scissor);

        vkCmdBindDescriptorSets(
            cmdbuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            mPipelineLayout,
            0, 1, &mDescriptorSet,
            0, nullptr
        );
        vkCmdBindPipeline(cmdbuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            mPipeline
        );

        constexpr const VkDeviceSize offset = 0;
        // TODO
        // if (mUI.background)
        // {
        //     vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &mModels.background.vertices.mBuffer, &offset);
        //     vkCmdBindIndexBuffer(cmdbuffer, &mModels.background.indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
        //     vkCmdDrawIndexed(cmdbuffer, &mModels.background.indexCount, 1, 0, 0, 0);
        // }

        // TODO
        // if (mUI.models)
        // {
        //     vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &mModels.models.vertices.mBuffer, &offset);
        //     vkCmdBindIndexBuffer(cmdbuffer, &mModels.models.indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
        //     vkCmdDrawIndexed(cmdbuffer, &mModels.models.indexCount, 1, 0, 0, 0);
        // }

        // TODO
        // if (mUI.logos)
        // {
        //     vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &mModels.logos.vertices.mBuffer, &offset);
        //     vkCmdBindIndexBuffer(cmdbuffer, &mModels.logos.indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
        //     vkCmdDrawIndexed(cmdbuffer, &mModels.logos.indexCount, 1, 0, 0, 0);
        // }

        build_imgui_command_buffer(cmdbuffer);

        vkCmdEndRenderPass(cmdbuffer);

        CHECK(vkEndCommandBuffer(cmdbuffer));
    }
}

void VulkanDearImGui::build_imgui_command_buffer(VkCommandBuffer cmdbuffer)
{
    ImGuiIO& io = ImGui::GetIO();

    const VkViewport viewport{
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = io.DisplaySize.x,
        .height   = io.DisplaySize.y,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    const Constants constants{
        .scale     = { 2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y },
        .translate = { -1.0f, -1.0f },
    };

    vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSet, 0, nullptr);
    vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
    vkCmdSetViewport(cmdbuffer, 0, 1, &viewport);
    vkCmdPushConstants(cmdbuffer, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);

    const ImDrawData* draw_data = ImGui::GetDrawData();

    constexpr const VkDeviceSize offset = 0;
    if (draw_data->CmdListsCount > 0)
    {
        vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &mVertex.buffer.mBuffer, &offset);

        vkCmdBindIndexBuffer(cmdbuffer, mIndex.buffer.mBuffer, offset, VK_INDEX_TYPE_UINT16);

        std::uint32_t offset_index = 0;
        std::int32_t  offset_vertex = 0;

        for (auto idx_list = 0, count_list = draw_data->CmdListsCount; idx_list < count_list; ++idx_list)
        {
            const ImDrawList* list = draw_data->CmdLists[idx_list];
            for (auto idx_buffer = 0, count_buffer = list->CmdBuffer.Size; idx_buffer < count_buffer; ++idx_buffer)
            {
                const ImDrawCmd& command = list->CmdBuffer[idx_buffer];

                const VkRect2D scissors{
                    .offset = VkOffset2D{
                        .x = std::max(static_cast<std::int32_t>(command.ClipRect.x), 0),
                        .y = std::max(static_cast<std::int32_t>(command.ClipRect.y), 0),
                    },
                    .extent = VkExtent2D{
                        .width  = static_cast<std::uint32_t>(command.ClipRect.z - command.ClipRect.x),
                        .height = static_cast<std::uint32_t>(command.ClipRect.w - command.ClipRect.y),
                    }
                };

                vkCmdSetScissor(cmdbuffer, 0, 1, &scissors);
                vkCmdDrawIndexed(cmdbuffer, command.ElemCount, 1, offset_index, offset_vertex, 0);

                offset_index += command.ElemCount;
            }
            offset_vertex += list->VtxBuffer.Size;
        }
    }
}

void VulkanDearImGui::new_frame(bool update_frame_times)
{
    auto clear_color = ImColor(114, 144, 154);

    ImGui::NewFrame();

    ImGui::TextUnformatted("theblackunknown - Playground");
    ImGui::TextUnformatted(mProperties.deviceName);

    // Update frame time display
    if (update_frame_times)
    {
        std::rotate(
            std::begin(mUI.frame_times), std::next(std::begin(mUI.frame_times)),
            std::end(mUI.frame_times)
        );

        float frame_time = 1e3 / (mBenchmark.frame_timer * 1e3);
        mUI.frame_times.back() = frame_time;
        if (frame_time < mUI.frame_time_min)
            mUI.frame_time_min = frame_time;
        if (frame_time > mUI.frame_time_max)
            mUI.frame_time_max = frame_time;

    }

    ImGui::PlotLines(
        "Frame Times",
        &mUI.frame_times[0], static_cast<int>(mUI.frame_times.size()),
        0,
        nullptr,
        mUI.frame_time_min,
        mUI.frame_time_max,
        ImVec2(0, 80)
    );

    ImGui::Text("Camera");
    ImGui::InputFloat3("position", mCamera.position, "%.2f");
    ImGui::InputFloat3("rotation", mCamera.rotation, "%.2f");

    ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Example Settings");
    ImGui::Checkbox   ("Render models"     , &mUI.models);
    ImGui::Checkbox   ("Display logos"     , &mUI.logos);
    ImGui::Checkbox   ("Display background", &mUI.background);
    ImGui::Checkbox   ("Animated lights"   , &mUI.animated_lights);
    ImGui::SliderFloat("Light Speed"       , &mUI.light_speed, 0.1f, 1.0f);
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
    ImGui::ShowDemoWindow();

    ImGui::Render();
}

void VulkanDearImGui::update_imgui_draw_data()
{
    const ImDrawData* draw_data = ImGui::GetDrawData();
    assert(draw_data->Valid);

    // TODO Check Alignment
    VkDeviceSize vertex_buffer_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize index_buffer_size  = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

    if ((vertex_buffer_size == 0) || (index_buffer_size == 0))
        return;

    // TODO Allocate a single memory, then bind vertex & index buffer to it
    if (mVertex.count != draw_data->TotalVtxCount)
    {
        if (mVertex.buffer.mBuffer != VK_NULL_HANDLE)
        {
            mVertex.buffer.deallocate();
        }
        mVertex.buffer.allocate(vertex_buffer_size);
        mVertex.count = draw_data->TotalVtxCount;

        ScopedBufferMapping<ImDrawVert> mapping(mVertex.buffer);
        ImDrawVert* mapped_memory = mapping.mMappedMemory;
        for(auto idx = 0, count = draw_data->CmdListsCount; idx < count; ++idx)
        {
            const ImDrawList* draw_list = draw_data->CmdLists[idx];
            std::copy(
                draw_list->VtxBuffer.Data, std::next(draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size),
                mapped_memory
            );
            mapped_memory += draw_list->VtxBuffer.Size;
        }
        const VkMappedMemoryRange range{
            .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .pNext  = nullptr,
            .memory = mVertex.buffer.mMemory,
            .offset = 0,
            .size   = VK_WHOLE_SIZE
        };
        vkFlushMappedMemoryRanges(mDevice, 1, &range);
    }
    if (mIndex.count != draw_data->TotalIdxCount)
    {
        if (mIndex.buffer.mBuffer != VK_NULL_HANDLE)
        {
            mIndex.buffer.deallocate();
        }
        // TODO NYI reallocate
        mIndex.buffer.allocate(index_buffer_size);
        mIndex.count = draw_data->TotalIdxCount;

        ScopedBufferMapping<ImDrawIdx> mapping(mIndex.buffer);
        ImDrawIdx* mapped_memory = mapping.mMappedMemory;
        for(auto idx = 0, count = draw_data->CmdListsCount; idx < count; ++idx)
        {
            const ImDrawList* draw_list = draw_data->CmdLists[idx];
            std::copy(
                draw_list->IdxBuffer.Data, std::next(draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size),
                mapped_memory
            );
            mapped_memory += draw_list->IdxBuffer.Size;
        }

        const VkMappedMemoryRange range{
            .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .pNext  = nullptr,
            .memory = mIndex.buffer.mMemory,
            .offset = 0,
            .size   = VK_WHOLE_SIZE
        };
        vkFlushMappedMemoryRanges(mDevice, 1, &range);
    }
}

void VulkanDearImGui::render_frame(VulkanSurface& surface)
{
    auto tick_start = std::chrono::high_resolution_clock::now();

    // TODO Veiw updated

    render(surface);
    ++mBenchmark.frame_counter;

    auto tick_end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(tick_end - tick_start).count();
    mBenchmark.frame_timer = static_cast<float>(duration / 1000.f);

    // TODO Camera
    // TODO Animation timer

    float fps_timer = static_cast<float>(std::chrono::duration<double, std::milli>(tick_end - mBenchmark.frame_tick).count());
    if (fps_timer > 1e3f)
    {
        mBenchmark.frame_per_seconds = static_cast<std::uint32_t>(mBenchmark.frame_counter * (1000.f / fps_timer));
        mBenchmark.frame_counter = 0;
        mBenchmark.frame_tick = tick_end;
    }
}

void VulkanDearImGui::render(VulkanSurface& surface)
{
    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2(surface.mResolution.width, surface.mResolution.height);
    io.DeltaTime   = mBenchmark.frame_timer;

    // TODO Mouse
    // io.MousePos     = ImVec2(mMouse.offset.x, mMouse.offset.y);
    // io.MouseDown[0] = mMouse.buttons.left;
    // io.MouseDown[1] = mMouse.buttons.right;

    // NOTE
    //  - [ ] the application must use a synchronization primitive to ensure that the presentation engine has finished reading from the image
    //  - [ ] The application can then transition the image’s layout
    //  - [ ] queue rendering commands to it
    //  - [ ] etc
    //  - [ ] Finally, the application presents the image with vkQueuePresentKHR, which releases the acquisition of the image.
    //  cf. https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/html/vkspec.html#_wsi_swapchain

    VkResult result_acquire = VK_SUCCESS;
    VkResult result_present = VK_SUCCESS;
    {
        const ScopedPresentableImage scoped_presentable_image(surface, result_acquire, result_present);
        if (result_acquire == VK_ERROR_OUT_OF_DATE_KHR)
        {
            invalidate_surface(surface);
        }
        else if (result_acquire == VK_SUBOPTIMAL_KHR)
        {
            invalidate_surface(surface);
        }
        else
        {
            CHECK(result_acquire);
        }

        // NOTE Done after in case, Swap Chain re-generated
        build_imgui_command_buffers(surface, scoped_presentable_image.mIndex);
        // build_imgui_command_buffers(surface);

        surface.submit(scoped_presentable_image.mIndex);
    }
    CHECK(result_present);
    if (result_present == VK_ERROR_OUT_OF_DATE_KHR)
    {
        invalidate_surface(surface);
    }
    else if (result_present == VK_SUBOPTIMAL_KHR)
    {
        invalidate_surface(surface);
    }
    else
    {
        CHECK(result_present);
    }

    // NOTE Can we avoid blocking ?
    CHECK(vkQueueWaitIdle(surface.mQueue));

    // TODO Animated Lights
    // if (mUI.animated_lights)
    //     (void);
}

void VulkanDearImGui::invalidate_surface(VulkanSurface& surface)
{
    // Ensure all operations on the device have been finished before destroying resources
    vkDeviceWaitIdle(mDevice);

    // Recreate swap chain
    surface.generate_swapchain(false);

    // Recreate Depth
    vkDestroyImageView(mDevice, mDepth.view, nullptr);
    vkDestroyImage(mDevice, mDepth.image, nullptr);
    vkFreeMemory(mDevice, mDepth.memory, nullptr);
    setup_depth(surface.mResolution, mDepth.format);

    // Recreate Framebuffers
    for (auto&& framebuffer : mFrameBuffers)
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    {
        std::vector<VkImageView> color_views;
        color_views.reserve(surface.mBuffers.size());
        for (auto&& buffer : surface.mBuffers)
            color_views.push_back(buffer.view);
        setup_framebuffers(surface.mResolution, color_views);
    }

    // Ensure all operations have completed before going further
    vkDeviceWaitIdle(mDevice);
}
