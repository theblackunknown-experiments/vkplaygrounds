#include <vulkan/vulkan.h>

#include <windows.h>
#include <shellapi.h>

#include <imgui.h>

#include <cassert>

#include <chrono>
#include <thread>

#include <map>
#include <set>
#include <string>
#include <vector>

#include <locale>
#include <codecvt>

#include <iostream>

#include <utilities.hpp>

#include "./vkdebug.hpp"
#include "./vkutilities.hpp"

#include "./vkqueue.hpp"
#include "./vkengine.hpp"
#include "./vksurface.hpp"
#include "./vkapplication.hpp"
#include "./vkpresentation.hpp"
#include "./vkphysicaldevice.hpp"

#include "./vkpass.hpp"

#include "./sample0/vksample0.hpp"

using namespace std::literals::chrono_literals;

namespace
{
    constexpr const VkExtent2D kResolution = { 1280, 720 };

    // NVIDIA - https://developer.nvidia.com/blog/vulkan-dos-donts/
    //  > Prefer using 24 bit depth formats for optimal performance
    //  > Prefer using packed depth/stencil formats. This is a common cause for notable performance differences between an OpenGL and Vulkan implementation.
    constexpr std::array kPreferredDepthFormats{
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D32_SFLOAT,
    };

    bool sResizing = false;

    // cf. https://stackoverflow.com/questions/215963/how-do-you-properly-use-widechartomultibyte

    // Convert a wide Unicode string to an UTF8 string
    std::string utf8_encode(const std::wstring &wstr)
    {
        #if 0
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.to_bytes(wstr);
        #else
            if( wstr.empty() ) return std::string();
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
            std::string strTo( size_needed, 0 );
            WideCharToMultiByte                  (CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
            return strTo;
        #endif
    }

    // Convert an UTF8 string to a wide Unicode String
    std::wstring utf8_decode(const std::string &str)
    {
        if( str.empty() ) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo( size_needed, 0 );
        MultiByteToWideChar                  (CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }
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
        for(int i{0}; i < argc; ++i)
            args.emplace_back(utf8_encode(wargv[i]));
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

    std::cout << "_MSC_VER        : " << _MSC_VER << std::endl;
    std::cout << "_MSC_FULL_VER   : " << _MSC_FULL_VER << std::endl;
    std::cout << "_MSC_BUILD      : " << _MSC_BUILD << std::endl;
    std::cout << "_MSVC_LANG      : " << _MSVC_LANG << std::endl;
    std::cout << "_MSC_EXTENSIONS : " << _MSC_EXTENSIONS << std::endl;

    static bool sGuard = false;
    while (sGuard)
        std::this_thread::sleep_for(5s);

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

    blk::Surface vksurface = blk::Surface::create(
        application, VkWin32SurfaceCreateInfoKHR{
            .sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext     = nullptr,
            .flags     = 0,
            .hinstance = hInstance,
            .hwnd      = hWindow,
        }
    );

    auto vkphysicaldevices = blk::physicaldevices(application);
    assert(vkphysicaldevices.size() > 0);

    std::cout << "Detected " << vkphysicaldevices.size() << " GPU:" << std::endl;
    for (auto vkphysicaldevice : vkphysicaldevices)
    {
        std::cout << "GPU: "<< vkphysicaldevice.mProperties.deviceName << " (" << DeviceType2Text(vkphysicaldevice.mProperties.deviceType) << ')' << std::endl;

        std::uint32_t major = VK_VERSION_MAJOR(vkphysicaldevice.mProperties.apiVersion);
        std::uint32_t minor = VK_VERSION_MINOR(vkphysicaldevice.mProperties.apiVersion);
        std::uint32_t patch = VK_VERSION_PATCH(vkphysicaldevice.mProperties.apiVersion);
        std::cout << '\t' << "API: " << major << "." << minor << "." << patch << std::endl;
        major = VK_VERSION_MAJOR(vkphysicaldevice.mProperties.driverVersion);
        minor = VK_VERSION_MINOR(vkphysicaldevice.mProperties.driverVersion);
        patch = VK_VERSION_PATCH(vkphysicaldevice.mProperties.driverVersion);
        std::cout << '\t' << "Driver: " << major << "." << minor << "." << patch << std::endl;
    }

    auto isSuitable = [&vksurface](const blk::PhysicalDevice& vkphysicaldevice) {
        // Engine requirements:
        //  - GRAPHICS queue
        // Presentation requirements:
        //  - Surface compatible queue

        if (vkphysicaldevice.mProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            return false;

        return std::ranges::any_of(
            vkphysicaldevice.mQueueFamilies,
            [&vksurface](const blk::QueueFamily& family) {
                if (!family.supports_presentation())
                    return false;

                if (!family.supports_surface(vksurface))
                    return false;

                return true;
            }
        );
    };

    auto finderSuitablePhysicalDevice = std::ranges::find_if(vkphysicaldevices, isSuitable);
    assert(finderSuitablePhysicalDevice != std::ranges::end(vkphysicaldevices));

    /*const */blk::PhysicalDevice& vkphysicaldevice(*finderSuitablePhysicalDevice);

    std::cout << "Selected GPU: " << vkphysicaldevice.mProperties.deviceName << std::endl;

    blk::Engine engine = [&application, &vkphysicaldevice]{
        std::uint32_t priorities_count = 0;
        for (auto&& queue_family : vkphysicaldevice.mQueueFamilies)
            priorities_count = std::max(priorities_count, queue_family.mProperties.queueCount);

        std::vector<float> priorities(priorities_count, 1.0f);
        std::vector<VkDeviceQueueCreateInfo> info_queues(vkphysicaldevice.mQueueFamilies.size());
        for (auto&& [info_queue, queue_family] : zip(info_queues, vkphysicaldevice.mQueueFamilies))
        {
            info_queue.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            info_queue.pNext            = nullptr;
            info_queue.flags            = 0;
            info_queue.queueFamilyIndex = queue_family.mIndex;
            info_queue.queueCount       = queue_family.mProperties.queueCount;
            info_queue.pQueuePriorities = priorities.data();
        }

        return blk::Engine(application, vkphysicaldevice, info_queues);
    }();

    blk::Presentation presentation(engine, vksurface, kResolution);

    blk::sample0::Sample sample(engine, presentation.mColorFormat, kResolution);

    #if 0
    DearImGuiengine engine(application, vksurface, vkphysicaldevice, kResolution);

    engine.allocate_memory_and_bind_resources();

    engine.initialize_views();

    engine.create_swapchain();
    engine.create_framebuffers();
    engine.create_commandbuffers();
    engine.create_graphic_pipelines();

    engine.upload_font_image();
    engine.update_descriptorset();
    #endif

    ShowWindow(hWindow, nCmdShow);
    SetForegroundWindow(hWindow);
    SetFocus(hWindow);

    SetWindowLongPtr(hWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&engine));
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
            #if 0
            engine.render_frame();
            #endif
        }
    }

    #if 0
    engine.wait_pending_operations();
    #endif

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
    blk::Engine* engine = reinterpret_cast<blk::Engine*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    assert(engine);

    switch (uMsg)
    {
    case WM_PAINT:
        // TODO Fetch RenderArea
        #if 0
        engine->render_frame();
        #endif
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
    #if 0
    case WM_LBUTTONDOWN:
        engine->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        engine->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        engine->mMouse.buttons.left = true;
        break;
    case WM_RBUTTONDOWN:
        engine->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        engine->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        engine->mMouse.buttons.right = true;
        break;
    case WM_MBUTTONDOWN:
        engine->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        engine->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        engine->mMouse.buttons.middle = true;
        break;
    case WM_LBUTTONUP:
        engine->mMouse.buttons.left = false;
        break;
    case WM_RBUTTONUP:
        engine->mMouse.buttons.right = false;
        break;
    case WM_MBUTTONUP:
        engine->mMouse.buttons.middle = false;
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
        engine->mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        engine->mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        break;
    }
    #endif
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
                    if (engine)
                    {
                        engine->invalidate_surface(*sSurface);
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
