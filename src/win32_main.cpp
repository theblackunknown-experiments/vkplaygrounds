#include <vulkan/vulkan.h>

#include <windows.h>
#include <shellapi.h>

#include <imgui.h>

#include <cassert>

#include <chrono>

#include <map>
#include <set>
#include <string>
#include <vector>

#include <locale>
#include <codecvt>

#include <iostream>

#include "./vkdebug.hpp"
#include "./vkutilities.hpp"
#include "./vksurface.hpp"
#include "./vkapplication.hpp"
#include "./win32_vkutilities.hpp"

#include "./dearimgui/dearimguishowcase.hpp"

namespace
{
    constexpr const VkExtent2D kResolution = { 1280, 720 };
    constexpr const bool       kVSync = true;

    bool sResizing = false;
}

static
LRESULT CALLBACK MinimalWindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static
LRESULT CALLBACK MainWindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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

    // Application - Instance

    std::uint32_t apiVersion;
    CHECK(vkEnumerateInstanceVersion(&apiVersion));
    // This playground is to experiment with Vulkan 1.2
    assert(apiVersion >= VK_MAKE_VERSION(1,2,0));

    VulkanApplication application(Version{ VK_MAKE_VERSION(1, 2, 0) });

    assert(has_extension(application.mExtensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME));

    HWND hWindow;
    {// Window
        WNDCLASSEX clazz{
            .cbSize        = sizeof(WNDCLASSEX),
            .style         = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc   = &MinimalWindowProcedure,
            .cbClsExtra    = 0,
            .cbWndExtra    = 0,
            .hInstance     = hInstance,
            .hIcon         = LoadIconA(nullptr, IDI_APPLICATION),
            .hCursor       = LoadCursor(nullptr, IDC_ARROW),
            .hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)),
            .lpszMenuName  = 0,
            .lpszClassName = "VkPlaygroundWindowClazz",
            .hIconSm       = LoadIconA(nullptr, IDI_WINLOGO),
        };

        ATOM clazzid = RegisterClassExA(&clazz);
        assert(clazzid != 0);

        RECT window_rectange{
            .left   = 0,
            .top    = 0,
            .right  = kResolution.width,
            .bottom = kResolution.height,
        };
        // WS_EX_OVERLAPPEDWINDOW
        DWORD window_style = WS_OVERLAPPEDWINDOW;
        // dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        // dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

        BOOL status = AdjustWindowRect(&window_rectange, window_style, FALSE);
        assert(status);
        hWindow = CreateWindowExA(
            0,
            "VkPlaygroundWindowClazz",
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
    }
    VulkanSurface surface(application.mInstance, create_surface(application.mInstance, hInstance, hWindow));

    auto vkphysicaldevices = physical_devices(application.mInstance);
    assert(vkphysicaldevices.size() > 0);

    std::cout << "Detected " << vkphysicaldevices.size() << " GPU:" << std::endl;
    for (auto vkphysicaldevice : vkphysicaldevices)
    {
        VkPhysicalDeviceProperties vkproperties;

        vkGetPhysicalDeviceProperties(vkphysicaldevice, &vkproperties);

        std::cout << "GPU: "<< vkproperties.deviceName << " (" << DeviceType2Text(vkproperties.deviceType) << ')' << std::endl;

        std::uint32_t major = VK_VERSION_MAJOR(vkproperties.apiVersion);
        std::uint32_t minor = VK_VERSION_MINOR(vkproperties.apiVersion);
        std::uint32_t patch = VK_VERSION_PATCH(vkproperties.apiVersion);
        std::cout << '\t' << "API: " << major << "." << minor << "." << patch << std::endl;
        major = VK_VERSION_MAJOR(vkproperties.driverVersion);
        minor = VK_VERSION_MINOR(vkproperties.driverVersion);
        patch = VK_VERSION_PATCH(vkproperties.driverVersion);
        std::cout << '\t' << "Driver: " << major << "." << minor << "." << patch << std::endl;
    }

    auto finderSuitablePhysicalDevice = std::find_if(
        std::begin(vkphysicaldevices), std::end(vkphysicaldevices),
        [&surface](VkPhysicalDevice vkphysicaldevice) {
            return DearImGuiShowcase::isSuitable(vkphysicaldevice, surface);
        }
    );
    assert(finderSuitablePhysicalDevice != std::end(vkphysicaldevices));

    auto vkphysicaldevice = *finderSuitablePhysicalDevice;
    assert(vkphysicaldevice != VK_NULL_HANDLE);

    {// Physical Devices
        VkPhysicalDeviceFeatures             vkfeatures;
        VkPhysicalDeviceProperties           vkproperties;
        VkPhysicalDeviceMemoryProperties     vkmemoryproperties;
        std::vector<VkQueueFamilyProperties> vkqueuefamiliesproperties;
        std::vector<VkExtensionProperties>   vkextensions;

        vkGetPhysicalDeviceFeatures(vkphysicaldevice, &vkfeatures);
        vkGetPhysicalDeviceProperties(vkphysicaldevice, &vkproperties);
        vkGetPhysicalDeviceMemoryProperties(vkphysicaldevice, &vkmemoryproperties);
        {// Queue Family Properties
            std::uint32_t count;
            vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, nullptr);
            assert(count > 0);
            vkqueuefamiliesproperties.resize(count);
            vkGetPhysicalDeviceQueueFamilyProperties(vkphysicaldevice, &count, vkqueuefamiliesproperties.data());
        }
        {// Extensions
            std::uint32_t count;
            CHECK(vkEnumerateDeviceExtensionProperties(vkphysicaldevice, nullptr, &count, nullptr));
            vkextensions.resize(count);
            CHECK(vkEnumerateDeviceExtensionProperties(vkphysicaldevice, nullptr, &count, vkextensions.data()));
        }

        std::cout << "Selected GPU: " << vkproperties.deviceName << std::endl;
    }

    DearImGuiShowcase showcase(application, surface, vkphysicaldevice, kResolution);

    showcase.initialize();
    showcase.initialize_resources();
    showcase.allocate_descriptorset();

    showcase.allocate_memory_and_bind_resources();

    showcase.initialize_views();

    showcase.create_swapchain();
    showcase.create_framebuffers();
    showcase.create_commandbuffers();
    showcase.create_graphic_pipelines();

    showcase.upload_font_image();
    showcase.update_descriptorset();

    ShowWindow(hWindow, nCmdShow);
    SetForegroundWindow(hWindow);
    SetFocus(hWindow);

    SetWindowLongPtr(hWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&showcase));
    SetWindowLongPtr(hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MainWindowProcedure));

    MSG msg = { };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else if(!IsIconic(hWindow))
        {
            showcase.render_frame();
        }
    }

    showcase.wait_pending_operations();

    FreeConsole();

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MinimalWindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_KEYUP:
        switch (wParam)
        {
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        }
        break;
    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK MainWindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    DearImGuiShowcase* showcase = reinterpret_cast<DearImGuiShowcase*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    assert(showcase);

    switch (uMsg)
    {
    case WM_PAINT:
        // TODO Fetch RenderArea
        showcase->render_frame();
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
        showcase->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        showcase->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        showcase->mMouse.buttons.left = true;
        break;
    case WM_RBUTTONDOWN:
        showcase->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        showcase->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        showcase->mMouse.buttons.right = true;
        break;
    case WM_MBUTTONDOWN:
        showcase->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        showcase->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        showcase->mMouse.buttons.middle = true;
        break;
    case WM_LBUTTONUP:
        showcase->mMouse.buttons.left = false;
        break;
    case WM_RBUTTONUP:
        showcase->mMouse.buttons.right = false;
        break;
    case WM_MBUTTONUP:
        showcase->mMouse.buttons.middle = false;
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
        showcase->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        showcase->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        break;
    }
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            if ((sResizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
            {
                #if 0
                // TODO Update Camera
                if (sSurface)
                {
                    sSurface->mResolution = VkExtent2D{
                        .width  = LOWORD(lParam),
                        .height = HIWORD(lParam)
                    };
                    if (showcase)
                    {
                        showcase->invalidate_surface(*sSurface);
                    }
                    else
                    {
                        sSurface->generate_swapchain();
                    }
                }
                #endif
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
    default:
        return MinimalWindowProcedure(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}
