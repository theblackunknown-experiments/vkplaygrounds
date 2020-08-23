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
#include "./vulkanqueuefamilyindices.hpp"

#include "./win32_vkapplication.hpp"

#include "./vulkanengine.hpp"
#include "./vulkandearimgui.hpp"
#include "./vulkanpresentation.hpp"
#include "./vulkangenerativeshader.hpp"

namespace
{
    constexpr const VkExtent2D kWindowExtent = { 1280, 720 };
    constexpr const bool       kVSync = true;

    bool sResizing = false;
    // VulkanSurface* sSurface = nullptr;
    // VulkanDearImGui* sDearImGui = nullptr;
}

static
LRESULT CALLBACK MinimalWindocProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
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

    HWND hWindow;
    {// Window
        WNDCLASSEX clazz{
            .cbSize        = sizeof(WNDCLASSEX),
            .style         = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc   = &MinimalWindocProcedure,
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
            .right  = kWindowExtent.width,
            .bottom = kWindowExtent.height,
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

    // Application - Instance

    VulkanWin32Application application(hInstance, hWindow);
    // This playground is to experiment with Vulkan 1.2
    assert(VK_MAKE_VERSION(1,2,0) <= application.mVersion);

    VkPhysicalDevice                     vkphysicaldevice = VK_NULL_HANDLE;
    VkPhysicalDeviceFeatures             vkfeatures;
    VkPhysicalDeviceProperties           vkproperties;
    VkPhysicalDeviceMemoryProperties     vkmemoryproperties;
    std::vector<VkQueueFamilyProperties> vkqueuefamiliesproperties;
    std::vector<VkExtensionProperties>   vkextensions;

    {// Physical Devices
        auto vkphysicaldevices = physical_devices(application.mInstance);
        assert(vkphysicaldevices.size() > 0);

        std::cout << "Detected " << vkphysicaldevices.size() << " GPU:" << std::endl;
        for (auto vkphysicaldevice : vkphysicaldevices)
        {
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

        auto finder = std::find_if(
            std::begin(vkphysicaldevices), std::end(vkphysicaldevices),
            [vksurface = application.mSurface](VkPhysicalDevice vkphysicaldevice) {
                return VulkanEngine::supports(vkphysicaldevice) &&
                    VulkanDearImGui::supports(vkphysicaldevice) &&
                    VulkanPresentation::supports(vkphysicaldevice, vksurface) &&
                    VulkanGenerativeShader::supports(vkphysicaldevice);
            }
        );
        assert(finder != std::end(vkphysicaldevices));

        vkphysicaldevice = *finder;

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
    assert(vkphysicaldevice != VK_NULL_HANDLE);

    // Queues
    VulkanQueueFamilyIndices vkqueuefamilyindices;

    auto [queue_family_index_engine, queue_count_engine] = VulkanEngine::requirements(vkphysicaldevice);
    auto [queue_family_index_dearimgui, queue_count_dearimgui] = VulkanDearImGui::requirements(vkphysicaldevice);
    auto [queue_family_index_presentation, queue_count_presentation] = VulkanPresentation::requirements(vkphysicaldevice, application.mSurface);
    auto [queue_family_index_generativeshader, queue_count_generativeshader] = VulkanGenerativeShader::requirements(vkphysicaldevice);

    vkqueuefamilyindices.engine = queue_family_index_engine;
    vkqueuefamilyindices.dearimgui = queue_family_index_dearimgui;
    vkqueuefamilyindices.presentation = queue_family_index_presentation;
    vkqueuefamilyindices.generativeshader = queue_family_index_generativeshader;

    assert(queue_count_engine > 0);
    assert(queue_count_dearimgui > 0);
    assert(queue_count_presentation > 0);
    assert(queue_count_generativeshader > 0);

    // NOTE Dummy but at least provides enough priorities in worst case
    const auto max_queue_count(
        std::max(
            std::max(queue_count_engine, queue_count_dearimgui),
            std::max(queue_count_presentation, queue_count_generativeshader)
        )
    );
    const auto total_queue_count =
        queue_count_engine + queue_count_dearimgui +
        queue_count_presentation + queue_count_generativeshader
    ;
    const std::set<std::uint32_t> unique_indices{
        vkqueuefamilyindices.engine,
        vkqueuefamilyindices.dearimgui,
        vkqueuefamilyindices.presentation,
        vkqueuefamilyindices.generativeshader
    };

    const std::vector<float> priorities(max_queue_count, 1.0f);
    std::vector<VkDeviceQueueCreateInfo> info_queue_creations;
    for (std::uint32_t queue_family_index : unique_indices)
    {
        std::set<std::uint32_t> expected_queue_count;
        if (queue_family_index == vkqueuefamilyindices.engine)
            expected_queue_count.insert(queue_count_engine);
        if (queue_family_index == vkqueuefamilyindices.dearimgui)
            expected_queue_count.insert(queue_count_dearimgui);
        if (queue_family_index == vkqueuefamilyindices.presentation)
            expected_queue_count.insert(queue_count_presentation);
        if (queue_family_index == vkqueuefamilyindices.generativeshader)
            expected_queue_count.insert(queue_count_generativeshader);

        const std::uint32_t queue_count = *std::min_element(
            std::begin(expected_queue_count), std::end(expected_queue_count)
        );
        info_queue_creations.push_back(VkDeviceQueueCreateInfo{
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .queueFamilyIndex = queue_family_index,
            .queueCount       = queue_count,
            .pQueuePriorities = priorities.data(),
        });
    }

    VkDevice vkdevice = VK_NULL_HANDLE;
    {// Device
        constexpr const VkPhysicalDeviceFeatures features{
            .robustBufferAccess                      = VK_FALSE,
            .fullDrawIndexUint32                     = VK_FALSE,
            .imageCubeArray                          = VK_FALSE,
            .independentBlend                        = VK_FALSE,
            .geometryShader                          = VK_FALSE,
            .tessellationShader                      = VK_FALSE,
            .sampleRateShading                       = VK_FALSE,
            .dualSrcBlend                            = VK_FALSE,
            .logicOp                                 = VK_FALSE,
            .multiDrawIndirect                       = VK_FALSE,
            .drawIndirectFirstInstance               = VK_FALSE,
            .depthClamp                              = VK_FALSE,
            .depthBiasClamp                          = VK_FALSE,
            .fillModeNonSolid                        = VK_FALSE,
            .depthBounds                             = VK_FALSE,
            .wideLines                               = VK_FALSE,
            .largePoints                             = VK_FALSE,
            .alphaToOne                              = VK_FALSE,
            .multiViewport                           = VK_FALSE,
            .samplerAnisotropy                       = VK_FALSE,
            .textureCompressionETC2                  = VK_FALSE,
            .textureCompressionASTC_LDR              = VK_FALSE,
            .textureCompressionBC                    = VK_FALSE,
            .occlusionQueryPrecise                   = VK_FALSE,
            .pipelineStatisticsQuery                 = VK_FALSE,
            .vertexPipelineStoresAndAtomics          = VK_FALSE,
            .fragmentStoresAndAtomics                = VK_FALSE,
            .shaderTessellationAndGeometryPointSize  = VK_FALSE,
            .shaderImageGatherExtended               = VK_FALSE,
            .shaderStorageImageExtendedFormats       = VK_FALSE,
            .shaderStorageImageMultisample           = VK_FALSE,
            .shaderStorageImageReadWithoutFormat     = VK_FALSE,
            .shaderStorageImageWriteWithoutFormat    = VK_FALSE,
            .shaderUniformBufferArrayDynamicIndexing = VK_FALSE,
            .shaderSampledImageArrayDynamicIndexing  = VK_FALSE,
            .shaderStorageBufferArrayDynamicIndexing = VK_FALSE,
            .shaderStorageImageArrayDynamicIndexing  = VK_FALSE,
            .shaderClipDistance                      = VK_FALSE,
            .shaderCullDistance                      = VK_FALSE,
            .shaderFloat64                           = VK_FALSE,
            .shaderInt64                             = VK_FALSE,
            .shaderInt16                             = VK_FALSE,
            .shaderResourceResidency                 = VK_FALSE,
            .shaderResourceMinLod                    = VK_FALSE,
            .sparseBinding                           = VK_FALSE,
            .sparseResidencyBuffer                   = VK_FALSE,
            .sparseResidencyImage2D                  = VK_FALSE,
            .sparseResidencyImage3D                  = VK_FALSE,
            .sparseResidency2Samples                 = VK_FALSE,
            .sparseResidency4Samples                 = VK_FALSE,
            .sparseResidency8Samples                 = VK_FALSE,
            .sparseResidency16Samples                = VK_FALSE,
            .sparseResidencyAliased                  = VK_FALSE,
            .variableMultisampleRate                 = VK_FALSE,
            .inheritedQueries                        = VK_FALSE,
        };
        std::vector<const char*> enabled_extensions{};
        // TODO Fint out why this is needed
        if (has_extension(vkextensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
        {// Debug Marker
            enabled_extensions.emplace_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        }
        const VkDeviceCreateInfo info{
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .queueCreateInfoCount    = static_cast<std::uint32_t>(info_queue_creations.size()),
            .pQueueCreateInfos       = info_queue_creations.data(),
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            .enabledExtensionCount   = static_cast<std::uint32_t>(enabled_extensions.size()),
            .ppEnabledExtensionNames = enabled_extensions.data(),
            .pEnabledFeatures        = &features,
        };
        CHECK(vkCreateDevice(vkphysicaldevice, &info, nullptr, &vkdevice));
    }

    std::unordered_map<std::uint32_t, std::vector<VkQueue>> queues_by_family;
    for (const VkDeviceQueueCreateInfo& info_queue_creation : info_queue_creations)
    {
        std::vector<VkQueue>& queues = queues_by_family[info_queue_creation.queueFamilyIndex];
        queues.resize(info_queue_creation.queueCount);
        for (std::uint32_t idx = 0; idx < info_queue_creation.queueCount; ++idx)
            vkGetDeviceQueue(vkdevice, info_queue_creation.queueFamilyIndex, idx, &queues.at(idx));
    }

    // Components

    std::vector<VkQueue> queues_engine(queue_count_engine);
    std::vector<VkQueue> queues_dearimgui(queue_count_dearimgui);
    std::vector<VkQueue> queues_presentation(queue_count_presentation);
    std::vector<VkQueue> queues_generativeshader(queue_count_generativeshader);

    for (auto&& iterator : queues_by_family)
    {
        const std::uint32_t&        queue_family_index = iterator.first;
        const std::vector<VkQueue>& queues             = iterator.second;

        if (queue_family_index == vkqueuefamilyindices.engine)
            std::copy_n(std::begin(queues), queue_count_engine, std::begin(queues_engine));
        if (queue_family_index == vkqueuefamilyindices.dearimgui)
            std::copy_n(std::begin(queues), queue_count_dearimgui, std::begin(queues_dearimgui));
        if (queue_family_index == vkqueuefamilyindices.presentation)
            std::copy_n(std::begin(queues), queue_count_presentation, std::begin(queues_presentation));
        if (queue_family_index == vkqueuefamilyindices.generativeshader)
            std::copy_n(std::begin(queues), queue_count_generativeshader, std::begin(queues_generativeshader));
    }

    VulkanPresentation vkpresentation(
        vkphysicaldevice, vkdevice,
        vkqueuefamilyindices.presentation, queues_presentation,
        application.mSurface, kWindowExtent, kVSync
    );

    VulkanGenerativeShader vkgenerativeshader(
        vkphysicaldevice, vkdevice,
        vkqueuefamilyindices.generativeshader, queues_generativeshader
    );

    // VulkanDearImGui vkdearimgui(
    //     vkphysicaldevice, vkdevice,
    //     vkqueuefamilyindices.dearimgui, queues_dearimgui,
    //     &vkpresentation
    // );

    VulkanEngine vkengine(
        vkphysicaldevice, vkdevice,
        vkqueuefamilyindices.engine, queues_engine,
        &vkpresentation
    );

    #if 0
    {// Font Uploading
        auto [vkextent, data] = vkdearimgui.get_font_data_8bits();

        const VkDeviceSize vksize = vkextent.width * vkextent.height * sizeof(unsigned char);

        auto [vkstagingbuffer, vkstagingmemory] = vkdearimgui.create_staging_font_buffer(vksize);

        {// Memory Mapping
            constexpr const VkDeviceSize offset = 0;
            constexpr const VkDeviceSize size   = VK_WHOLE_SIZE;

            unsigned char* mapped_memory = nullptr;
            CHECK(vkMapMemory(
                vkdearimgui.mDevice,
                vkstagingmemory, offset, size, 0, reinterpret_cast<void**>(&mapped_memory)));

            std::copy(data, std::next(data, size), mapped_memory);

            vkUnmapMemory(vkdearimgui.mDevice, vkstagingmemory);

            const VkMappedMemoryRange range{
                .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .pNext  = nullptr,
                .memory = vkstagingmemory,
                .offset = offset,
                .size   = size
            };
            CHECK(vkFlushMappedMemoryRanges(vkdearimgui.mDevice, 1, &range));
        }
        {// Command Buffer
            VkFence vkfence;
            VkCommandBuffer vkcmdbuffer;

            {// Command Buffer
                const VkCommandBufferAllocateInfo info{
                    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .pNext              = nullptr,
                    .commandPool        = vkdearimgui.mCommandPool,
                    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    .commandBufferCount = 1,
                };
                CHECK(vkAllocateCommandBuffers(vkdearimgui.mDevice, &info, &vkcmdbuffer));
            }
            {// Fence
                const VkFenceCreateInfo info{
                    .sType              = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                    .pNext              = nullptr,
                    .flags              = 0,
                };
                CHECK(vkCreateFence(vkdearimgui.mDevice, &info, nullptr, &vkfence));
            }
            {// Start
                const VkCommandBufferBeginInfo info{
                    .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .pNext            = nullptr,
                    .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                    .pInheritanceInfo = nullptr,
                };
                CHECK(vkBeginCommandBuffer(vkcmdbuffer, &info));
            }

            vkdearimgui.record_font_optimal_image(vkextent ,vkcmdbuffer, vkstagingbuffer);

            {// End
                CHECK(vkEndCommandBuffer(vkcmdbuffer));
            }
            {// Submission
                const VkSubmitInfo info{
                    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .pNext                = nullptr,
                    .waitSemaphoreCount   = 0,
                    .pWaitSemaphores      = nullptr,
                    .pWaitDstStageMask    = nullptr,
                    .commandBufferCount   = 1,
                    .pCommandBuffers      = &vkcmdbuffer,
                    .signalSemaphoreCount = 0,
                    .pSignalSemaphores    = nullptr,
                };
            }

            CHECK(vkWaitForFences(vkdearimgui.mDevice, 1, &vkfence, VK_TRUE, 10e9));

            vkDestroyFence(vkdearimgui.mDevice, vkfence, nullptr);
            vkFreeCommandBuffers(vkdearimgui.mDevice, vkdearimgui.mCommandPool, 1, &vkcmdbuffer);
        }

        vkdearimgui.destroy_staging_font_buffer(vkstagingbuffer, vkstagingmemory);
    }
    #endif

    vkpresentation.recreate_swapchain();

    vkengine.recreate_depth(vkpresentation.mResolution);

    // vkdearimgui.recreate_graphics_pipeline(vkpresentation.mResolution);

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

    ShowWindow(hWindow, nCmdShow);
    SetForegroundWindow(hWindow);
    SetFocus(hWindow);

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
    vkDestroyDevice(vkdevice, nullptr);
    FreeConsole();

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MinimalWindocProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        // TODO Ask confirmation
        DestroyWindow(hWnd);
        PostQuitMessage(0);
        break;
    #if 0
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
    #endif
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        // TODO Ask confirmation
        DestroyWindow(hWnd);
        PostQuitMessage(0);
        break;
    #if 0
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
    #endif
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
