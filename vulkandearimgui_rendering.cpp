#include <vulkan/vulkan.h>

#include <imgui.h>

#include <chrono>
#include <iterator>
#include <algorithm>

// #include <iostream>

#include "./vulkandebug.hpp"

#include "./vulkanbuffer.hpp"
#include "./vulkansurface.hpp"
#include "./vulkanscopedbuffermapping.hpp"
#include "./vulkanscopedpresentableimage.hpp"

#include "./vulkandearimgui.hpp"

using namespace std::literals::chrono_literals;

void VulkanDearImGui::upload_imgui_frame_data()
{
    const ImDrawData* data = ImGui::GetDrawData();
    assert(data->Valid);

    if (data->TotalVtxCount == 0)
        return;

    // TODO Check Alignment
    VkDeviceSize vertex_buffer_size = data->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize index_buffer_size  = data->TotalIdxCount * sizeof(ImDrawIdx);

    // TODO Allocate a single memory, then bind vertex & index buffer to it
    if (mVertex.count != data->TotalVtxCount)
    {
        if (mVertex.buffer.mBuffer != VK_NULL_HANDLE)
        {
            mVertex.buffer.deallocate();
        }
        mVertex.buffer.allocate(vertex_buffer_size);
        mVertex.count = data->TotalVtxCount;
    }
    if (mIndex.count != data->TotalIdxCount)
    {
        if (mIndex.buffer.mBuffer != VK_NULL_HANDLE)
        {
            mIndex.buffer.deallocate();
        }
        mIndex.buffer.allocate(index_buffer_size);
        mIndex.count = data->TotalIdxCount;
    }
    {// Map Memory
        ScopedBufferMapping<ImDrawVert> mapping_vertex(mVertex.buffer);
        ScopedBufferMapping<ImDrawIdx>  mapping_index(mIndex.buffer);

        ImDrawVert* memory_vertex = mapping_vertex.mMappedMemory;
        ImDrawIdx*  memory_index  = mapping_index.mMappedMemory;

        for(auto idx = 0, count = data->CmdListsCount; idx < count; ++idx)
        {
            const ImDrawList* list = data->CmdLists[idx];
            std::copy(list->VtxBuffer.Data, std::next(list->VtxBuffer.Data, list->VtxBuffer.Size), memory_vertex);
            std::copy(list->IdxBuffer.Data, std::next(list->IdxBuffer.Data, list->IdxBuffer.Size), memory_index);
            memory_vertex += list->VtxBuffer.Size;
            memory_index  += list->IdxBuffer.Size;
        }
        const VkMappedMemoryRange ranges[] = {
            VkMappedMemoryRange{
                .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .pNext  = nullptr,
                .memory = mVertex.buffer.mMemory,
                .offset = 0,
                .size   = VK_WHOLE_SIZE
            },
            VkMappedMemoryRange{
                .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .pNext  = nullptr,
                .memory = mIndex.buffer.mMemory,
                .offset = 0,
                .size   = VK_WHOLE_SIZE
            },
        };
        vkFlushMappedMemoryRanges(mDevice, IM_ARRAYSIZE(ranges), ranges);
    }
}

void VulkanDearImGui::render_frame(VulkanSurface& surface)
{
    auto tick_start = clock_type_t::now();

    // TODO Veiw updated

    render(surface);
    ++mBenchmark.frame_counter;

    auto tick_end = clock_type_t::now();
    auto duration = std::chrono::duration<double, std::milli>(tick_end - tick_start).count();
    mBenchmark.frame_timer = static_cast<float>(duration / 1000.f);

    // TODO Camera
    // TODO Animation timer

    float fps_timer = static_cast<float>(std::chrono::duration<double, std::milli>(tick_end - mBenchmark.frame_tick).count());
    if (fps_timer > 1e3f)
    {
        mBenchmark.frame_per_seconds = static_cast<std::uint32_t>(mBenchmark.frame_counter * (1000.f / fps_timer));
        // std::cout << "FPS update: " << mBenchmark.frame_per_seconds << std::endl;
        mBenchmark.frame_counter = 0;
        mBenchmark.frame_tick = tick_end;
    }
}

void VulkanDearImGui::render(VulkanSurface& surface)
{
    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2(surface.mResolution.width, surface.mResolution.height);
    io.DeltaTime   = mBenchmark.frame_timer;

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
        record_presentableimage_commandbuffer(surface, scoped_presentable_image.mIndex);

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

void VulkanDearImGui::record_presentableimage_commandbuffer(VulkanSurface& surface, std::uint32_t idx)
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

    declare_imgui_frame(mBenchmark.frame_counter == 0);
    upload_imgui_frame_data();

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

        record_presentableimage_commandbuffer_imgui(cmdbuffer);

        vkCmdEndRenderPass(cmdbuffer);

        CHECK(vkEndCommandBuffer(cmdbuffer));
    }
}

void VulkanDearImGui::record_presentableimage_commandbuffer_imgui(VkCommandBuffer cmdbuffer)
{
    const ImDrawData* data = ImGui::GetDrawData();

    const VkExtent2D resolution{
        .width  = static_cast<std::uint32_t>(data->DisplaySize.x * data->FramebufferScale.x),
        .height = static_cast<std::uint32_t>(data->DisplaySize.y * data->FramebufferScale.y),
    };

    if (resolution.width == 0 || resolution.height == 0)
        return;

    const VkViewport viewport{
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = data->DisplaySize.x * data->FramebufferScale.x,
        .height   = data->DisplaySize.y * data->FramebufferScale.y,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    {// Pipeline & Descriptor Sets
        vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
        vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSet, 0, nullptr);
    }
    {// Viewport
        assert(viewport.width > 0);
        assert(viewport.height > 0);

        vkCmdSetViewport(cmdbuffer, 0, 1, &viewport);
    }

    {// Constants
        Constants constants{};
        constants.scale    [0] =  2.0f / data->DisplaySize.x;
        constants.scale    [1] =  2.0f / data->DisplaySize.y;
        constants.translate[0] = -1.0f - data->DisplayPos.x * constants.scale[0];
        constants.translate[1] = -1.0f - data->DisplayPos.y * constants.scale[1];
        vkCmdPushConstants(cmdbuffer, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants), &constants);
    }

    if (data->TotalVtxCount > 0)
    {// Buffer Bindings
        constexpr const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &mVertex.buffer.mBuffer, &offset);
        vkCmdBindIndexBuffer(cmdbuffer, mIndex.buffer.mBuffer, offset, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    }

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

void VulkanDearImGui::invalidate_surface(VulkanSurface& surface)
{
    // Ensure all operations on the device have been finished before destroying resources
    vkDeviceWaitIdle(mDevice);

    // Recreate swap chain
    surface.generate_swapchain();

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
