

#include <chrono>
#include <limits>
#include <thread>

#include <map>
#include <set>
#include <string>

#include <vkdebug.hpp>
#include <vkutilities.hpp>

#include <vkqueue.hpp>

#include <vkpass.hpp>

///////////////////////////////////////////////////

#include "./console.hpp"
#include "./charconv.hpp"

#include "./win32_console.hpp"
#include "./win32_windowprocedure.hpp"

#include "./vksurface.hpp"
#include "./vkphysicaldevice.hpp"

#include "./vkengine.hpp"
#include "./vkpresentation.hpp"

#include "./vklauncher.hpp"
#include "./vkapplication.hpp"

#include <vkcpumesh.hpp>

#include <range/v3/view/zip.hpp>

#include <Windows.h>

#include <vulkan/vulkan.h>

#include <cassert>

#include <vector>

#include <iostream>

#include <obj2mesh.hpp>

#include <mesh-color-shader.hpp>
#include <mesh-position-shader.hpp>
#include <mesh-normal-shader.hpp>

#include <glm/gtx/transform.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace
{
constexpr const VkExtent2D kResolution = {1280, 720};
constexpr auto kTimeoutAcquirePresentationImage = std::numeric_limits<std::uint64_t>::max();

struct WindowUserData
{
    blk::Engine& engine;
    blk::Presentation& presentation;
    blk::sample00::Application& application;
    bool& shutting_down;
    bool& resizing;
    bool default_mesh = true;
};
} // namespace

static LRESULT CALLBACK MainWindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    blk::ConsoleHolder console_holder("sample00");

    std::cout << "_MSC_VER        : " << _MSC_VER << std::endl;
    std::cout << "_MSC_FULL_VER   : " << _MSC_FULL_VER << std::endl;
    std::cout << "_MSC_BUILD      : " << _MSC_BUILD << std::endl;
    std::cout << "_MSVC_LANG      : " << _MSVC_LANG << std::endl;
    std::cout << "_MSC_EXTENSIONS : " << _MSC_EXTENSIONS << std::endl;

    std::vector<std::string> args = blk::cmdline_to_args(pCmdLine);

    // Application - Instance
    blk::sample00::Launcher vkinstance;

    HWND hWindow;
    { // Window
        WNDCLASSEX clazz{
            .cbSize = sizeof(WNDCLASSEX),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = &blk::MinimalWindowProcedure,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = hInstance,
            .hIcon = LoadIconA(nullptr, IDI_APPLICATION),
            .hCursor = LoadCursor(nullptr, IDC_ARROW),
            .hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)),
            .lpszMenuName = 0,
            .lpszClassName = "VkPlaygroundsWindowClazz",
            .hIconSm = LoadIconA(nullptr, IDI_WINLOGO),
        };

        ATOM clazzid = RegisterClassExA(&clazz);
        assert(clazzid != 0);

        RECT window_rectange{
            .left = 0,
            .top = 0,
            .right = kResolution.width,
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
            "VkPlaygroundsWindowClazz",
            "sample00",
            window_style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            window_rectange.right - window_rectange.left,
            window_rectange.bottom - window_rectange.top,
            nullptr,
            nullptr,
            hInstance,
            nullptr);
        assert(hWindow);
    }

    blk::Surface vksurface = blk::Surface::create(
        vkinstance,
        VkWin32SurfaceCreateInfoKHR{
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .hinstance = hInstance,
            .hwnd = hWindow,
        });

    auto vkphysicaldevices = blk::physicaldevices(vkinstance);
    assert(vkphysicaldevices.size() > 0);

    std::cout << "Found " << vkphysicaldevices.size() << " GPU:" << std::endl;
    for (auto vkphysicaldevice : vkphysicaldevices)
    {
        std::cout << "GPU: " << vkphysicaldevice.mProperties.deviceName << " ("
                  << DeviceType2Text(vkphysicaldevice.mProperties.deviceType) << ')' << std::endl;

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
        if (vkphysicaldevice.mProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            return false;

        return std::ranges::any_of(vkphysicaldevice.mQueueFamilies, [&vksurface](const blk::QueueFamily& family) {
            if (!family.supports_presentation())
                return false;

            if (!family.supports_surface(vksurface))
                return false;

            return true;
        });
    };

    auto finder_physical_device = std::ranges::find_if(vkphysicaldevices, isSuitable);
    assert(finder_physical_device != std::ranges::end(vkphysicaldevices));

    blk::PhysicalDevice& vkphysicaldevice(*finder_physical_device);

    std::cout << "Selected GPU: " << vkphysicaldevice.mProperties.deviceName << std::endl;

    MSG msg = {};
    {
        blk::Engine engine = [&vkinstance, &vkphysicaldevice] {
            std::uint32_t priorities_count = 0;
            for (auto&& queue_family : vkphysicaldevice.mQueueFamilies)
                priorities_count = std::max(priorities_count, queue_family.mProperties.queueCount);

            std::vector<float> priorities(priorities_count, 1.0f);
            std::vector<VkDeviceQueueCreateInfo> info_queues(vkphysicaldevice.mQueueFamilies.size());
            for (auto&& [info_queue, queue_family] : ranges::views::zip(info_queues, vkphysicaldevice.mQueueFamilies))
            {
                info_queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                info_queue.pNext = nullptr;
                info_queue.flags = 0;
                info_queue.queueFamilyIndex = queue_family.mIndex;
                info_queue.queueCount = queue_family.mProperties.queueCount;
                info_queue.pQueuePriorities = priorities.data();
            }

            return blk::Engine(vkinstance, vkphysicaldevice, info_queues);
        }();

        blk::Presentation presentation(engine, vksurface, kResolution);

        blk::sample00::Application application(engine, presentation);

        application.load_meshes(
            {0, 1, 2},
            std::array{
                blk::CPUMesh{.mVertices{
                    blk::Vertex{.position = {+1.0, +1.0, +0.0}, .color = {0.0, 1.0, 0.0}},
                    blk::Vertex{.position = {-1.0, +1.0, +0.0}, .color = {0.0, 1.0, 0.0}},
                    blk::Vertex{.position = {+0.0, -1.0, +0.0}, .color = {0.0, 1.0, 0.0}},
                }},
                blk::meshes::obj::load(
                    fs::path("C:\\devel\\vkplaygrounds\\data\\models\\vulkan-guide\\monkey_smooth.obj")),
                blk::meshes::obj::load(fs::path("C:\\Users\\machizau\\Downloads\\Donuts Tutorial.obj")),
            });

        application.load_pipelines(
            {0, 1, 2},
            std::array{"mesh_position_main", "mesh_normal_main", "mesh_color_main"},
            std::array{
                std::span<const std::uint32_t>(kShaderMeshPosition),
                std::span<const std::uint32_t>(kShaderMeshNormal),
                std::span<const std::uint32_t>(kShaderMeshColor)});

        { // Setup 0
            application.load_renderables(std::array{blk::Renderable{
                .mMesh = application.mStorageMesh.mMeshes[0].get(),
                .mMaterial = std::addressof(application.mStorageMaterial.mMaterials[0]),
                .transform = glm::identity<glm::mat4>(),
            }});
        }

        bool shutting_down = false;
        bool resizing = false;
        WindowUserData user_data{engine, presentation, application, shutting_down, resizing};

        ShowWindow(hWindow, nCmdShow);
        SetForegroundWindow(hWindow);
        SetFocus(hWindow);

        SetWindowLongPtr(hWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&user_data));
        SetWindowLongPtr(hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(MainWindowProcedure));

        while ((msg.message != WM_QUIT) && !shutting_down)
        {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            if (!IsIconic(hWindow))
            {
                application.draw();
            }
        }

        vkDeviceWaitIdle(engine.mDevice);
    }

    vksurface.destroy();
    DestroyWindow(hWindow);

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    WindowUserData* userdata = reinterpret_cast<WindowUserData*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    assert(userdata);

    blk::Engine& engine = userdata->engine;
    blk::Presentation& presentation = userdata->presentation;
    blk::sample00::Application& application = userdata->application;

    constexpr float kCameraStep = 0.1f;

    switch (uMsg)
    {
    case WM_QUIT:
    case WM_CLOSE:
    case WM_DESTROY: {
        userdata->shutting_down = true;
        break;
    }
    case WM_KEYDOWN:
        switch (wParam)
        {
        case 'Q': {
            static int sMesh = 0;

            sMesh = (sMesh + 1) % 3;

            switch (sMesh)
            {
            case 0:
            default:
                application.schedule_before_render_command([&application] {
                    constexpr std::uint32_t kTriangle = 0;

                    constexpr std::uint32_t kPosition = 0;

                    application.load_renderables(std::array{blk::Renderable{
                        .mMesh = application.mStorageMesh.mMeshes[kTriangle].get(),
                        .mMaterial = std::addressof(application.mStorageMaterial.mMaterials[kPosition]),
                        .transform = glm::identity<glm::mat4>(),
                    }});
                });

                break;
            }
        case 1:
            application.schedule_before_render_command([&application] {
                const blk::GPUMesh* mesh_triangle = application.mStorageMesh.mMeshes[0].get();
                const blk::GPUMesh* mesh_monkey = application.mStorageMesh.mMeshes[1].get();

                const blk::Material* material_position = std::addressof(application.mStorageMaterial.mMaterials[0]);

                std::vector<blk::Renderable> renderables;

                renderables.push_back(blk::Renderable{
                    .mMesh = mesh_monkey,
                    .mMaterial = material_position,
                    .transform = glm::identity<glm::mat4>(),
                });

                for (int x = -20; x <= 20; ++x)
                {
                    for (int y = -20; y <= 20; ++y)
                    {
                        glm::mat4 translation = glm::translate(glm::vec3(x, y, 0));
                        glm::mat4 scale = glm::scale(glm::vec3(0.2));

                        renderables.push_back(blk::Renderable{
                            .mMesh = mesh_triangle,
                            .mMaterial = material_position,
                            .transform = translation * scale,
                        });
                    }
                }

                application.load_renderables(renderables);
            });

            break;
            // case 1:
            //     application.schedule_before_render_command([&application] {
            //         application.load_mesh(blk::meshes::obj::load(
            //             fs::path("C:\\devel\\vkplaygrounds\\data\\models\\vulkan-guide\\monkey_smooth.obj")));
            //     });
            //     break;
            // case 2:
            //     application.schedule_before_render_command([&application] {
            //         application.load_mesh(
            //             blk::meshes::obj::load(fs::path("C:\\Users\\machizau\\Downloads\\Donuts Tutorial.obj")));
            //     });
            //     break;
        }
        break;
        case 'W': {
            application.mCamera.position.z += kCameraStep;
        }
        break;
        case 'A': {
            application.mCamera.position.x -= kCameraStep;
        }
        break;
        case 'S': {
            application.mCamera.position.z -= kCameraStep;
        }
        break;
        case 'D': {
            application.mCamera.position.x += kCameraStep;
        }
        break;
            break;
        }
        break;
    case WM_SIZE:
        switch (wParam)
        {
        case SIZE_RESTORED:
            if (!userdata->resizing)
                return blk::MinimalWindowProcedure(hWnd, uMsg, wParam, lParam);
            // case SIZE_MAXIMIZED: {
            //  VkExtent2D new_resolution{.width = LOWORD(lParam), .height = HIWORD(lParam)};

            //  // Ensure all operations on the device have been finished before destroying resources
            //  vkDeviceWaitIdle(engine.mDevice);

            //  presentation.onResize(new_resolution);

            //  application.onResize(presentation.mResolution);
            //  application.recreate_backbuffers(presentation.mColorFormat, presentation.mImages);
            //  return 0;
            // }
        }
    case WM_GETMINMAXINFO: {
        LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
        minMaxInfo->ptMinTrackSize.x = kResolution.width;
        minMaxInfo->ptMinTrackSize.y = kResolution.height;
        return 0;
    }
    case WM_ENTERSIZEMOVE: userdata->resizing = true; return 0;
    case WM_EXITSIZEMOVE: userdata->resizing = false; return 0;
    }
    return blk::MinimalWindowProcedure(hWnd, uMsg, wParam, lParam);
}
