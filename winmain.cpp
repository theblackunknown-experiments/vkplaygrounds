#include <vulkan/vulkan.h>

#include <windows.h>
#include <shellapi.h>

#include <cassert>

#include <vector>
#include <string>

#include <locale>
#include <codecvt>

#include <iostream>

#include "vulkandebug.hpp"
#include "vulkanapplication.hpp"
#include "vulkanphysicaldevice.hpp"
#include "vulkandevice.hpp"
#include "vulkansurface.hpp"
#include "vulkandearimgui.hpp"

namespace
{
    constexpr const VkExtent2D kDimension = { 512, 512 };

    LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        // sApplication->handleMessages(hWnd, uMsg, wParam, lParam);

        switch (uMsg)
        {
        case WM_CLOSE:
            // TODO Ask confirmation
            DestroyWindow(hWnd);
            PostQuitMessage(0);
            break;
        // case WM_PAINT:
        //     ValidateRect(window, NULL);
        //     break;
        // case WM_KEYDOWN:
        //     switch (wParam)
        //     {
        //     case KEY_P:
        //         paused = !paused;
        //         break;
        //     case KEY_F1:
        //         if (settings.overlay) {
        //             UIOverlay.visible = !UIOverlay.visible;
        //         }
        //         break;
        //     case KEY_ESCAPE:
        //         PostQuitMessage(0);
        //         break;
        //     }

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
        //     break;
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
        // case WM_LBUTTONDOWN:
        //     mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        //     mouseButtons.left = true;
        //     break;
        // case WM_RBUTTONDOWN:
        //     mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        //     mouseButtons.right = true;
        //     break;
        // case WM_MBUTTONDOWN:
        //     mousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        //     mouseButtons.middle = true;
        //     break;
        // case WM_LBUTTONUP:
        //     mouseButtons.left = false;
        //     break;
        // case WM_RBUTTONUP:
        //     mouseButtons.right = false;
        //     break;
        // case WM_MBUTTONUP:
        //     mouseButtons.middle = false;
        //     break;
        // case WM_MOUSEWHEEL:
        // {
        //     short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        //     zoom += (float)wheelDelta * 0.005f * zoomSpeed;
        //     camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f * zoomSpeed));
        //     viewUpdated = true;
        //     break;
        // }
        // case WM_MOUSEMOVE:
        // {
        //     handleMouseMove(LOWORD(lParam), HIWORD(lParam));
        //     break;
        // }
        // case WM_SIZE:
        //     if ((prepared) && (wParam != SIZE_MINIMIZED))
        //     {
        //         if ((resizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
        //         {
        //             destWidth = LOWORD(lParam);
        //             destHeight = HIWORD(lParam);
        //             windowResize();
        //         }
        //     }
        //     break;
        // case WM_GETMINMAXINFO:
        // {
        //     LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
        //     minMaxInfo->ptMinTrackSize.x = 64;
        //     minMaxInfo->ptMinTrackSize.y = 64;
        //     break;
        // }
        // case WM_ENTERSIZEMOVE:
        //     resizing = true;
        //     break;
        // case WM_EXITSIZEMOVE:
        //     resizing = false;
        //     break;
        }
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

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

    VkExtent2D resolution = kDimension;

    VulkanApplication app;
    assert(app.has_extension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME));

    // FIXME Arbitrary Physical Device selection
    auto vkphysicaldevices = app.physical_devices();
    assert(vkphysicaldevices.size() > 0);
    VkPhysicalDevice& vkphysicaldevice = vkphysicaldevices.at(0);

    VulkanPhysicalDevice gpu(vkphysicaldevice);
    // TODO VK_QUEUE_GRAPHICS_BIT arbitrary
    auto [queue_family_index, vkdevice] = gpu.create_device(VK_QUEUE_GRAPHICS_BIT, true);

    VulkanDevice device(app.mInstance, vkphysicaldevice, vkdevice, queue_family_index);

    BOOL status;
    HWND window_handle;
    {// register window
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
            .right  = kDimension.width,
            .bottom = kDimension.height,
        };
        // WS_EX_OVERLAPPEDWINDOW
        DWORD window_style = WS_OVERLAPPEDWINDOW;
        // dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        // dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

        status = AdjustWindowRect(&window_rectange, window_style, FALSE);
        assert(status);
        window_handle = CreateWindowExA(
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
        assert(window_handle);

        ShowWindow(window_handle, nCmdShow);
        SetForegroundWindow(window_handle);
        SetFocus(window_handle);
    }

    VkSurfaceKHR vksurface = VK_NULL_HANDLE;
    {// surface
        VkWin32SurfaceCreateInfoKHR info_surface{
            .sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext     = nullptr,
            .flags     = 0,
            .hinstance = hInstance,
            .hwnd      = window_handle,
        };

        VkResult err = vkCreateWin32SurfaceKHR(app.mInstance, &info_surface, nullptr, &vksurface);
        assert(err == VK_SUCCESS);
    }
    assert(vksurface != VK_NULL_HANDLE);

    VulkanSurface surface(app.mInstance, vkphysicaldevice, vkdevice, vksurface);

    VulkanDearImGui imgui(app.mInstance, vkphysicaldevice, vkdevice);

    VkFormat framebuffer_color_format = surface.mColorFormat;

    resolution = surface.generate_swapchain(resolution, false);

    // Depth Format
    auto optional_format = imgui.select_best_depth_format();
    assert(optional_format);
    VkFormat depth_format = optional_format.value();

    imgui.setup_render_pass(surface.mColorFormat, depth_format);

    std::vector<VkImageView> color_views(surface.mBuffers.size());
    for (auto&& buffer : surface.mBuffers)
        color_views.push_back(buffer.view);
    imgui.setup_framebuffers(resolution, color_views);

    imgui.setup_queues(queue_family_index);
    imgui.setup_depth(resolution, depth_format);
    imgui.setup_font();
    imgui.setup_shaders();
    imgui.setup_descriptors();
    imgui.setup_graphics_pipeline(resolution);

    MSG msg = { };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else if(!IsIconic(window_handle))
        {
            imgui.render_frame();
        }
    }

    vkDeviceWaitIdle(vkdevice);

    return static_cast<int>(msg.wParam);
}
