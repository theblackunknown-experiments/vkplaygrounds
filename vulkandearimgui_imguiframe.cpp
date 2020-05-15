
#include <imgui.h>

#include <iterator>
#include <algorithm>

#include "./vulkandearimgui.hpp"

void VulkanDearImGui::declare_imgui_frame(bool update_frame_times)
{
    ImGuiIO& io = ImGui::GetIO();

    io.MousePos     = ImVec2(mMouse.offset.x, mMouse.offset.y);
    io.MouseDown[ImGuiMouseButton_Left  ] = mMouse.buttons.left;
    io.MouseDown[ImGuiMouseButton_Right ] = mMouse.buttons.right;
    io.MouseDown[ImGuiMouseButton_Middle] = mMouse.buttons.middle;

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
        // std::cout << "Frame times: ";
        // std::copy(
        //     std::begin(mUI.frame_times), std::end(mUI.frame_times),
        //     std::ostream_iterator<float>(std::cout, ", ")
        // );
        // std::cout << std::endl;
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

    // ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
    // ImGui::Begin("Example Settings");
    // ImGui::Checkbox   ("Render models"     , &mUI.models);
    // ImGui::Checkbox   ("Display logos"     , &mUI.logos);
    // ImGui::Checkbox   ("Display background", &mUI.background);
    // ImGui::Checkbox   ("Animated lights"   , &mUI.animated_lights);
    // ImGui::SliderFloat("Light Speed"       , &mUI.light_speed, 0.1f, 1.0f);
    // ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
    ImGui::ShowDemoWindow();

    ImGui::Render();
}
