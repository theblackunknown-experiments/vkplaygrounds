
#include <imgui.h>

#include <iterator>
#include <algorithm>

#include "./vulkandearimgui.hpp"

constexpr const char* kWindowTitle = "theblackunknown - Playground";

void VulkanDearImGui::declare_imgui_frame(bool update_frame_times)
{
    ImGuiIO& io = ImGui::GetIO();

    io.MousePos                           = ImVec2(mMouse.offset.x, mMouse.offset.y);
    io.MouseDown[ImGuiMouseButton_Left  ] = mMouse.buttons.left;
    io.MouseDown[ImGuiMouseButton_Right ] = mMouse.buttons.right;
    io.MouseDown[ImGuiMouseButton_Middle] = mMouse.buttons.middle;

    auto clear_color = ImColor(114, 144, 154);

    static bool show_gpu_information = false;
    static bool show_fps = false;

    ImGui::NewFrame();
    {// Window
        const ImDrawList* list_bg = ImGui::GetBackgroundDrawList();
        const ImDrawList* list_fg = ImGui::GetForegroundDrawList();
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("About"))
            {
                ImGui::MenuItem("GPU Information", "", &show_gpu_information);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        // ImGui::Begin(kWindowTitle);
        // ImGui::End();

        if (show_gpu_information)
        {
            ImGui::Begin("GPU Information");

            ImGui::TextUnformatted(mProperties.deviceName);

            // TODO Do it outside ImGui frame
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
                auto [find_min, find_max] = std::minmax_element(std::begin(mUI.frame_times), std::end(mUI.frame_times));
                mUI.frame_time_min = *find_min;
                mUI.frame_time_max = *find_max;
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

            ImGui::End();
        }
    }

    // ImGui::Text("Camera");
    // ImGui::InputFloat3("position", mCamera.position, "%.2f");
    // ImGui::InputFloat3("rotation", mCamera.rotation, "%.2f");

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
