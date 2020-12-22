#include <vulkan/vulkan.h>

#include <windows.h>
#include <shellapi.h>

#include <cassert>

#include <limits>
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

#include "./vksample0.hpp"

namespace
{
    constexpr const VkExtent2D kResolution = { 1280, 720 };
    constexpr auto kTimeoutAcquirePresentationImage = std::numeric_limits<std::uint64_t>::max();

    struct WindowUserData
    {
        blk::Engine& engine;
        blk::Presentation& presentation;
        blk::sample0::Sample& sample;
        bool& ready;
        bool& shutting_down;
        bool& resizing;
    };

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
    using namespace std::literals::chrono_literals;

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

    struct ConsoleHolder
    {
        ConsoleHolder()
        {
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

        ~ConsoleHolder()
        {
            FreeConsole();
        }
    };

    ConsoleHolder console_holder;

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

    blk::sample0::Sample sample(engine, presentation.mColorFormat, presentation.mImages, kResolution);

    bool ready = false;
    bool shutting_down = false;
    bool resizing = false;
    WindowUserData user_data{engine, presentation, sample, ready, shutting_down, resizing};

    ShowWindow(hWindow, nCmdShow);
    SetForegroundWindow(hWindow);
    SetFocus(hWindow);

    SetWindowLongPtr(hWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&user_data));
    SetWindowLongPtr(hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MainWindowProcedure));

    auto& passui = sample.mPassUIOverlay;
    auto& passscene = sample.mPassScene;

    {// Font Image
        const blk::Queue* queue = passui.mGraphicsQueue;

        engine.submit(
            *passui.mGraphicsQueue,
            { passui.mFontImageStagingCommandBuffer },
            { },
            { },
            { passui.mFontImageStagingSemaphore },
            passui.mFontImageStagingFence);

        // TODO Do not block on that fence, inject the font image update semaphore into later queue submission for synchronization
        vkWaitForFences(engine.mDevice, 1, &(passui.mFontImageStagingFence), VK_TRUE, std::numeric_limits<std::uint64_t>::max());

        passui.clear_font_image_transient_data();
    }

    ready = true;
    const blk::Queue* presentation_queue = presentation.mPresentationQueues.at(0);

    MSG msg = { };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (ready)
            sample.onIdle();

            if (!shutting_down)
            {
                if(!IsIconic(hWindow))
                {
                    blk::Presentation::Image presentation_image = presentation.acquire_next(kTimeoutAcquirePresentationImage);
                    sample.record(presentation_image.index, presentation_image.commandbuffer);

                    VkSemaphore render_semaphore = sample.mRenderSemaphores.at(presentation_image.index);

                    // TODO Figure out how we can use different queue for sample computations and presentation job
                    //  This probably means that we need to record computations with commandbuffers than ones from presentation
                    engine.submit(
                        *presentation_queue,
                        { presentation_image.commandbuffer },
                        { presentation_image.semaphore },
                        { presentation_image.destination_stage_mask },
                        { render_semaphore }
                    );

                    auto result_present = presentation.present(presentation_image, render_semaphore);
                    // NOTE We block to avoid override something in use (e.g. surface indexed VkCommandBuffer)
                    // TODO Block per command buffer, instead of a global lock for all of them
                    CHECK(vkQueueWaitIdle(*presentation_queue));

                    if ((result_present == VK_SUBOPTIMAL_KHR) || (result_present == VK_ERROR_OUT_OF_DATE_KHR))
                    {
                        vkDeviceWaitIdle(engine.mDevice);
                        auto new_resolution = presentation.recreate_swapchain();

                        if (std::memcmp(&(sample.mResolution), &(new_resolution), sizeof(new_resolution)) != 0)
                        {
                            sample.onResize(new_resolution);
                        }

                        sample.recreate_backbuffers(presentation.mColorFormat, presentation.mImages);
                    }
                }
            }
    }

    vkDeviceWaitIdle(engine.mDevice);

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MinimalWindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;;
    case WM_KEYUP:
        switch (wParam)
        {
        case VK_ESCAPE:
            PostQuitMessage(0);
            return 0;;
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
        }
    default:
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }

}

LRESULT CALLBACK MainWindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WindowUserData* userdata = reinterpret_cast<WindowUserData*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    assert(userdata);

    blk::Engine& engine = userdata->engine;
    blk::Presentation& presentation = userdata->presentation;
    blk::sample0::Sample& sample = userdata->sample;

    auto& passui = sample.mPassUIOverlay;
    auto& passscene = sample.mPassScene;

    switch (uMsg)
    {
    case WM_CREATE:
        {
            userdata->ready = true;
            return MinimalWindowProcedure(hWnd, uMsg, wParam, lParam);
        }
    case WM_QUIT:
    case WM_CLOSE:
    case WM_DESTROY:
        {
            userdata->ready = false;
            userdata->shutting_down = true;
            return MinimalWindowProcedure(hWnd, uMsg, wParam, lParam);
        }
    case WM_PAINT:
        {
            // NOTE To make UI pass aware of resize changes
            sample.onIdle();

            blk::Presentation::Image presentation_image = presentation.acquire_next(kTimeoutAcquirePresentationImage);
            sample.record(presentation_image.index, presentation_image.commandbuffer);

            VkSemaphore render_semaphore = sample.mRenderSemaphores.at(presentation_image.index);

            // TODO Figure out how we can use different queue for sample computations and presentation job
            //  This probably means that we need to record computations with commandbuffers than ones from presentation
            const blk::Queue* presentation_queue = presentation.mPresentationQueues.at(0);
            engine.submit(
                *presentation_queue,
                { presentation_image.commandbuffer },
                { presentation_image.semaphore },
                { presentation_image.destination_stage_mask },
                { render_semaphore }
            );

            auto result_present = presentation.present(presentation_image, render_semaphore);
            // NOTE We block to avoid override something in use (e.g. surface indexed VkCommandBuffer)
            // TODO Block per command buffer, instead of a global lock for all of them
            CHECK(vkQueueWaitIdle(*presentation_queue));

            if ((result_present == VK_SUBOPTIMAL_KHR) || (result_present == VK_ERROR_OUT_OF_DATE_KHR))
            {
                vkDeviceWaitIdle(engine.mDevice);
                auto new_resolution = presentation.recreate_swapchain();

                if (std::memcmp(&(sample.mResolution), &(new_resolution), sizeof(new_resolution)) != 0)
                {
                    sample.onResize(new_resolution);
                }

                sample.recreate_backbuffers(presentation.mColorFormat, presentation.mImages);
            }

            ValidateRect(hWnd, NULL);
            return 0;
        }
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
        default:
            return MinimalWindowProcedure(hWnd, uMsg, wParam, lParam);
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
        passui.mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        passui.mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        passui.mMouse.buttons.left = true;
        return 0;
    case WM_RBUTTONDOWN:
        passui.mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        passui.mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        passui.mMouse.buttons.right = true;
        return 0;
    case WM_MBUTTONDOWN:
        passui.mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        passui.mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        passui.mMouse.buttons.middle = true;
        return 0;
    case WM_LBUTTONUP:
        passui.mMouse.buttons.left = false;
        return 0;
    case WM_RBUTTONUP:
        passui.mMouse.buttons.right = false;
        return 0;
    case WM_MBUTTONUP:
        passui.mMouse.buttons.middle = false;
        return 0;
    case WM_MOUSEWHEEL:
        passui.mMouse.wheel.vdelta += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
        return 0;
    case WM_MOUSEHWHEEL:
        passui.mMouse.wheel.hdelta += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
        return 0;
    case WM_MOUSEMOVE:
    {
        passui.mMouse.offset.x = static_cast<float>(LOWORD(lParam));
        passui.mMouse.offset.y = static_cast<float>(HIWORD(lParam));
        return 0;
    }
    case WM_SIZE:
        switch(wParam)
        {
            case SIZE_RESTORED:
                if (!userdata->resizing)
                    return MinimalWindowProcedure(hWnd, uMsg, wParam, lParam);
            case SIZE_MAXIMIZED:
                {
                    VkExtent2D new_resolution{.width  = LOWORD(lParam), .height = HIWORD(lParam)};

                    // Ensure all operations on the device have been finished before destroying resources
                    vkDeviceWaitIdle(engine.mDevice);

                    presentation.onResize(new_resolution);

                    sample.onResize(presentation.mResolution);
                    sample.recreate_backbuffers(presentation.mColorFormat, presentation.mImages);
                    return 0;
                }
            case SIZE_MINIMIZED:
            case SIZE_MAXSHOW:
            case SIZE_MAXHIDE:
            default:
                return MinimalWindowProcedure(hWnd, uMsg, wParam, lParam);
        }
    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
        minMaxInfo->ptMinTrackSize.x = kResolution.width;
        minMaxInfo->ptMinTrackSize.y = kResolution.height;
        return 0;
    }
    case WM_ENTERSIZEMOVE:
        userdata->resizing = true;
        return 0;
    case WM_EXITSIZEMOVE:
        userdata->resizing = false;
        return 0;
    default:
        return MinimalWindowProcedure(hWnd, uMsg, wParam, lParam);
    }
}
