#include <vulkan/vulkan.h>

#include <windows.h>
#include <shellapi.h>

#include <imgui.h>

#include <cassert>

#include <chrono>

#include <vector>
#include <string>

#include <locale>
#include <codecvt>

#include <iostream>

#include "./vulkandebug.hpp"
#include "./vulkanutilities.hpp"

#include "./vulkanapplication.hpp"
#include "./vulkanphysicaldevice.hpp"
#include "./vulkandevice.hpp"

#include "./vulkandearimgui.hpp"
#include "./vulkanpresentation.hpp"
#include "./vulkangenerativeshader.hpp"

namespace
{
    constexpr const VkExtent2D kWindowExtent = { 1280, 720 };
    constexpr const bool       kVSync = true;

    bool sResizing = false;
    VulkanSurface* sSurface = nullptr;
    VulkanDearImGui* sDearImGui = nullptr;
}

static
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    std::vector<std::string> args;
    {// command line
        int argc;
        LPWSTR* wargv = CommandLineToArgvW(pCmdLine, &argc);
        assert(wargv != nullptr);
        args.reserve(argc);
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        for(int i{0}; i < argc; ++i)
            args.emplace_back(converter.to_bytes(wargv[i]));
        LocalFree(wargv);
    }
    {// console
        auto success = AllocConsole();
        assert(success);
        SetConsoleTitle(TEXT("theblackunknown - Playground"));

        // std::cout, std::clog, std::cerr, std::cin
        FILE* dummy;
        freopen_s(&dummy, "CONIN$" , "r", stdin);
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);

        std::cout.clear();
        std::clog.clear();
        std::cerr.clear();
        std::cin.clear();

        // std::wcout, std::wclog, std::wcerr, std::wcin
        HANDLE hConOut = CreateFile(
            "CONOUT$",
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        HANDLE hConIn = CreateFile(
            "CONIN$",
            GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        SetStdHandle(STD_INPUT_HANDLE, hConIn);
        SetStdHandle(STD_OUTPUT_HANDLE, hConOut);
        SetStdHandle(STD_ERROR_HANDLE, hConOut);
        std::wcout.clear();
        std::wclog.clear();
        std::wcerr.clear();
        std::wcin.clear();
    }

    BOOL status;
    HWND hWindow;
    {// Window
        WNDCLASSEX clazz{
            .cbSize        = sizeof(WNDCLASSEX),
            .style         = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc   = &WndProc,
            .cbClsExtra    = 0,
            .cbWndExtra    = 0,
            .hInstance     = hInstance,
            .hIcon         = LoadIconA(nullptr, IDI_APPLICATION),
            .hCursor       = LoadCursor(nullptr, IDC_ARROW),
            .hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)),
            .lpszMenuName  = 0,
            .lpszClassName = "BlkDearImguiWindowClazz",
            .hIconSm       = LoadIconA(nullptr, IDI_WINLOGO),
        };

        ATOM clazzid = RegisterClassExA(&clazz);
        assert(clazzid != 0);

        RECT window_rectange{
            .left   = 0,
            .top    = 0,
            .right  = kWindowExtent.width,
            .bottom = kWindowExtent.height,
        };
        // WS_EX_OVERLAPPEDWINDOW
        DWORD window_style = WS_OVERLAPPEDWINDOW;
        // dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        // dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

        status = AdjustWindowRect(&window_rectange, window_style, FALSE);
        assert(status);
        hWindow = CreateWindowExA(
            0,
            "BlkDearImguiWindowClazz",
            "theblackunknown - Playground",
            window_style,
            CW_USEDEFAULT, CW_USEDEFAULT,
            window_rectange.right - window_rectange.left,
            window_rectange.bottom - window_rectange.top,
            nullptr,
            nullptr,
            hInstance,
            nullptr
        );
        assert(hWindow);

        ShowWindow(hWindow, nCmdShow);
        SetForegroundWindow(hWindow);
        SetFocus(hWindow);
    }

    // Application - Instance

    VulkanApplication application;
    assert(has_extension(application.mExtensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME));

    // Surface

    const VkWin32SurfaceCreateInfoKHR info_surface{
        .sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext     = nullptr,
        .flags     = 0,
        .hinstance = hInstance,
        .hwnd      = hWindow,
    };
    VkSurfaceKHR vksurface;
    CHECK(vkCreateWin32SurfaceKHR(application.mInstance, &info_surface, nullptr, &vksurface));

    // Physical Devices

    auto vkphysicaldevices = physical_devices(application.mInstance);
    assert(vkphysicaldevices.size() > 0);

    // Requirements

    auto [vkphysicaldevice_dearimgui, queue_family_index_dearimgui, queue_count_dearimgui] = VulkanDearImGui::requirements(vkphysicaldevices);
    auto [vkphysicaldevice_presentation, queue_family_index_presentation, queue_count_presentation] = VulkanPresentation::requirements(vkphysicaldevices, vksurface);
    auto [vkphysicaldevice_generativeshader, queue_family_index_generativeshader, queue_count_generativeshader] = VulkanGenerativeShader::requirements(vkphysicaldevices);

    assert(vkphysicaldevice_dearimgui);
    assert(vkphysicaldevice_generativeshader);

    VulkanDearImGui vkdearimgui(
        vkphysicaldevice_dearimgui,
        queue_family_index_dearimgui, queue_count_dearimgui
    );

    VulkanPresentation vkpresentation(
        vkphysicaldevice_presentation,
        queue_family_index_presentation, queue_count_presentation,
        vksurface, kWindowExtent, kVSync
    );

    VulkanGenerativeShader vkgenerativeshader(
        vkphysicaldevice_generativeshader,
        queue_family_index_generativeshader, queue_count_generativeshader
    );

    #if 0
    VulkanDevice device(application.mInstance, vkphysicaldevice, vkdevice, queue_family_index);


    // TODO Current limitation because we use a single device for now
    assert(queue_family_index == surface.mQueueFamilyIndex);

    VulkanDearImGui imgui(application.mInstance, vkphysicaldevice, vkdevice);
    ImGui::GetIO().ImeWindowHandle = hWindow;
    sDearImGui = &imgui;

    // VulkanGenerativeShader generative_shader(application.mInstance, vkphysicaldevice, vkdevice);

    VkFormat framebuffer_color_format = surface.mColorFormat;

    surface.generate_swapchain();

    // Depth Format
    auto optional_format = imgui.select_best_depth_format();
    assert(optional_format);
    VkFormat depth_format = optional_format.value();

    imgui.setup_render_pass(surface.mColorFormat, depth_format);
    imgui.setup_depth(surface.mResolution, depth_format);

    std::vector<VkImageView> color_views;
    color_views.reserve(surface.mBuffers.size());
    for (auto&& buffer : surface.mBuffers)
        color_views.push_back(buffer.view);
    imgui.setup_framebuffers(surface.mResolution, color_views);

    // TODO Store the Graphics queue into the surface object
    imgui.setup_queues(queue_family_index);
    imgui.setup_font();
    imgui.setup_shaders();
    imgui.setup_descriptors();
    imgui.setup_graphics_pipeline(surface.mResolution);

    imgui.mBenchmark.frame_tick = std::chrono::high_resolution_clock::now();
    #endif

    MSG msg = { };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        #if 0
        else if(!IsIconic(hWindow))
        // if(!IsIconic(hWindow))
        {
            imgui.render_frame(surface);
        }
        #endif
    }

    #if 0
    vkDeviceWaitIdle(vkdevice);
    #endif
    vkDestroySurfaceKHR(application.mInstance, vksurface, nullptr);
    FreeConsole();

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
#if 0
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        // TODO Ask confirmation
        DestroyWindow(hWnd);
        PostQuitMessage(0);
        break;
    case WM_PAINT:
        if (!sDearImGui)
            break;

        if (!sSurface)
            break;

        sDearImGui->render_frame(*sSurface);
        ValidateRect(hWnd, NULL);
        break;
    case WM_KEYDOWN:
        switch (wParam)
        {
    //     case KEY_P:
    //         paused = !paused;
    //         break;
    //     case KEY_F1:
    //         if (settings.overlay) {
    //             UIOverlay.visible = !UIOverlay.visible;
    //         }
    //         break;
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        }

    //     if (camera.firstperson)
    //     {
    //         switch (wParam)
    //         {
    //         case KEY_W:
    //             camera.keys.up = true;
    //             break;
    //         case KEY_S:
    //             camera.keys.down = true;
    //             break;
    //         case KEY_A:
    //             camera.keys.left = true;
    //             break;
    //         case KEY_D:
    //             camera.keys.right = true;
    //             break;
    //         }
    //     }

    //     keyPressed((uint32_t)wParam);
        break;
    // case WM_KEYUP:
    //     if (camera.firstperson)
    //     {
    //         switch (wParam)
    //         {
    //         case KEY_W:
    //             camera.keys.up = false;
    //             break;
    //         case KEY_S:
    //             camera.keys.down = false;
    //             break;
    //         case KEY_A:
    //             camera.keys.left = false;
    //             break;
    //         case KEY_D:
    //             camera.keys.right = false;
    //             break;
    //         }
    //     }
    //     break;
    case WM_LBUTTONDOWN:
        if (!sDearImGui)
            break;

        sDearImGui->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        sDearImGui->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        sDearImGui->mMouse.buttons.left = true;
        break;
    case WM_RBUTTONDOWN:
        if (!sDearImGui)
            break;

        sDearImGui->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        sDearImGui->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        sDearImGui->mMouse.buttons.right = true;
        break;
    case WM_MBUTTONDOWN:
        if (!sDearImGui)
            break;

        sDearImGui->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        sDearImGui->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        sDearImGui->mMouse.buttons.middle = true;
        break;
    case WM_LBUTTONUP:
        if (!sDearImGui)
            break;

        sDearImGui->mMouse.buttons.left = false;
        break;
    case WM_RBUTTONUP:
        if (!sDearImGui)
            break;

        sDearImGui->mMouse.buttons.right = false;
        break;
    case WM_MBUTTONUP:
        if (!sDearImGui)
            break;

        sDearImGui->mMouse.buttons.middle = false;
        break;
    // case WM_MOUSEWHEEL:
    // {
    //     short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
    //     zoom += (float)wheelDelta * 0.005f * zoomSpeed;
    //     camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f * zoomSpeed));
    //     viewUpdated = true;
    //     break;
    // }
    case WM_MOUSEMOVE:
    {
        if (!sDearImGui)
            break;

        sDearImGui->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        sDearImGui->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        break;
    }
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            if ((sResizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
            {
                // TODO Update Camera
                if (sSurface)
                {
                    sSurface->mResolution = VkExtent2D{
                        .width  = LOWORD(lParam),
                        .height = HIWORD(lParam)
                    };
                    if (sDearImGui)
                    {
                        sDearImGui->invalidate_surface(*sSurface);
                    }
                    else
                    {
                        sSurface->generate_swapchain();
                    }
                }
            }
        }
        break;
    // case WM_GETMINMAXINFO:
    // {
    //     LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
    //     minMaxInfo->ptMinTrackSize.x = 64;
    //     minMaxInfo->ptMinTrackSize.y = 64;
    //     break;
    // }
    case WM_ENTERSIZEMOVE:
        sResizing = true;
        break;
    case WM_EXITSIZEMOVE:
        sResizing = false;
        break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
#endif
