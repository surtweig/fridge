#include "app.h"
#include "rom_loader.h"
#include "falcfrontend.h"
#include "vulkan_globals.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef APP_USE_VULKAN_DEBUG_REPORT
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
#endif

VkAllocationCallbacks* g_Allocator = nullptr;
VkInstance               g_Instance = VK_NULL_HANDLE;
VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
VkDevice                 g_Device = VK_NULL_HANDLE;
uint32_t                 g_QueueFamily = (uint32_t)-1;
VkQueue                  g_Queue = VK_NULL_HANDLE;
VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;

static void check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    std::fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        std::abort();
}

#ifdef APP_USE_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags,
                                                   VkDebugReportObjectTypeEXT objectType,
                                                   uint64_t object, size_t location,
                                                   int32_t messageCode,
                                                   const char* pLayerPrefix,
                                                   const char* pMessage, void* pUserData)
{
    (void)flags; (void)object; (void)location; (void)messageCode;
    (void)pUserData; (void)pLayerPrefix;
    std::fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n",
                 objectType, pMessage);
    return VK_FALSE;
}
#endif

static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties,
                                 const char* extension)
{
    for (const VkExtensionProperties& p : properties)
        if (std::strcmp(p.extensionName, extension) == 0)
            return true;
    return false;
}

static bool IsLayerAvailable(const ImVector<VkLayerProperties>& layers, const char* name)
{
    for (const VkLayerProperties& l : layers)
        if (std::strcmp(l.layerName, name) == 0)
            return true;
    return false;
}

static void SetupVulkan(ImVector<const char*> instance_extensions)
{
    VkResult err;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    uint32_t properties_count;
    ImVector<VkExtensionProperties> properties;
    vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
    properties.resize(properties_count);
    err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
    check_vk_result(err);

    if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
    {
        instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif

#ifdef APP_USE_VULKAN_DEBUG_REPORT
    bool have_validation = false;
    {
        uint32_t layer_count = 0;
        ImVector<VkLayerProperties> layers;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        layers.resize(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, layers.Data);
        have_validation = IsLayerAvailable(layers, "VK_LAYER_KHRONOS_validation");
        if (have_validation)
            std::fprintf(stderr, "[vulkan] validation layer: enabled\n");
        else
            std::fprintf(stderr,
                "[vulkan] validation layer not installed; running without it. "
                "Install vulkan-validation-layers for debug checks.\n");
    }
    if (have_validation)
    {
        const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = layers;
        instance_extensions.push_back("VK_EXT_debug_report");
    }
#endif

    create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
    create_info.ppEnabledExtensionNames = instance_extensions.Data;
    err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
    check_vk_result(err);

#ifdef APP_USE_VULKAN_DEBUG_REPORT
    if (g_DebugReport == VK_NULL_HANDLE)
    {
        auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)
            vkGetInstanceProcAddr(g_Instance, "vkCreateDebugReportCallbackEXT");
        if (f_vkCreateDebugReportCallbackEXT)
        {
            VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
            debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT
                                  | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            debug_report_ci.pfnCallback = debug_report;
            debug_report_ci.pUserData = nullptr;
            err = f_vkCreateDebugReportCallbackEXT(g_Instance, &debug_report_ci, g_Allocator, &g_DebugReport);
            check_vk_result(err);
            std::fprintf(stderr, "[vulkan] debug report callback installed\n");
        }
    }
#endif

    g_PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g_Instance);
    IM_ASSERT(g_PhysicalDevice != VK_NULL_HANDLE);

    g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_PhysicalDevice);
    IM_ASSERT(g_QueueFamily != (uint32_t)-1);

    {
        ImVector<const char*> device_extensions;
        device_extensions.push_back("VK_KHR_swapchain");

        uint32_t dev_props_count;
        ImVector<VkExtensionProperties> dev_props;
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &dev_props_count, nullptr);
        dev_props.resize(dev_props_count);
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &dev_props_count, dev_props.Data);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (IsExtensionAvailable(dev_props, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
            device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = g_QueueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo device_ci = {};
        device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_ci.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        device_ci.pQueueCreateInfos = queue_info;
        device_ci.enabledExtensionCount = (uint32_t)device_extensions.Size;
        device_ci.ppEnabledExtensionNames = device_extensions.Data;
        err = vkCreateDevice(g_PhysicalDevice, &device_ci, g_Allocator, &g_Device);
        check_vk_result(err);
        vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
    }

    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_SAMPLER,       IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;
        pool_info.poolSizeCount = (uint32_t)IM_COUNTOF(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
        check_vk_result(err);
    }
}

static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface,
                              int width, int height)
{
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, surface, &res);
    if (res != VK_TRUE)
    {
        std::fprintf(stderr, "Error no WSI support on physical device 0\n");
        std::exit(-1);
    }

    const VkFormat requestSurfaceImageFormat[] = {
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,   VK_FORMAT_R8G8B8_UNORM
    };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->Surface = surface;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat,
        (size_t)IM_COUNTOF(requestSurfaceImageFormat), requestSurfaceColorSpace);

    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        g_PhysicalDevice, wd->Surface, &present_modes[0], IM_COUNTOF(present_modes));

    IM_ASSERT(g_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device,
                                           wd, g_QueueFamily, g_Allocator,
                                           width, height, g_MinImageCount, 0);
}

static void CleanupVulkan()
{
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);

#ifdef APP_USE_VULKAN_DEBUG_REPORT
    auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)
        vkGetInstanceProcAddr(g_Instance, "vkDestroyDebugReportCallbackEXT");
    if (f_vkDestroyDebugReportCallbackEXT)
        f_vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);
#endif

    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow(ImGui_ImplVulkanH_Window* wd)
{
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, wd, g_Allocator);
    vkDestroySurfaceKHR(g_Instance, wd->Surface, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
    VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkResult err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX,
                                        image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        g_SwapChainRebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return;
    if (err != VK_SUBOPTIMAL_KHR)
        check_vk_result(err);

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
        check_vk_result(err);
        err = vkResetFences(g_Device, 1, &fd->Fence);
        check_vk_result(err);
    }
    {
        err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
        check_vk_result(err);
    }
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd)
{
    if (g_SwapChainRebuild)
        return;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(g_Queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
        g_SwapChainRebuild = true;
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
        return;
    if (err != VK_SUBOPTIMAL_KHR)
        check_vk_result(err);
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
}

static int SmokeTest(const char* rom_path)
{
    FridgeCtx ctx;
    FridgeCtx_Init(ctx);

    if (!RomLoader_LoadRaw(&ctx.cpu, rom_path))
        return 1;

    std::printf("[smoke] loaded %s\n", rom_path);
    std::printf("[smoke] PC=%04X  A=%02X  state=%d  PANIC=%d\n",
                ctx.cpu.PC, ctx.cpu.rA, (int)ctx.cpu.state,
                (int)FRIDGE_cpu_flag_PANIC(&ctx.cpu));

    int steps = 0;
    const int max_steps = 1000000;
    while (ctx.cpu.state == FRIDGE_CPU_ACTIVE && steps < max_steps)
    {
        FRIDGE_sys_tick(&ctx.system);
        ++steps;
    }

    std::printf("[smoke] halted after %d steps: PC=%04X  A=%02X  state=%d  PANIC=%d\n",
                steps, ctx.cpu.PC, ctx.cpu.rA, (int)ctx.cpu.state,
                (int)FRIDGE_cpu_flag_PANIC(&ctx.cpu));

    unsigned non_zero = 0;
    for (int i = 0; i < FRIDGE_GPU_FRAME_BUFFER_SIZE; ++i)
        if (ctx.gpu.frame_a[i] != 0) ++non_zero;
    std::printf("[smoke] frame_a non-zero bytes: %u / %d\n",
                non_zero, FRIDGE_GPU_FRAME_BUFFER_SIZE);

    FridgeCtx_Shutdown(ctx);
    return 0;
}

static int SmokeTestSource(const char* source_path)
{
    DebugInfo info;
    std::string err;
    if (!FalcCompile(source_path, info, err))
    {
        std::fprintf(stderr, "[smoke-source] compile failed:\n%s\n", err.c_str());
        return 1;
    }

    std::printf("[smoke-source] compiled %s — %zu bytes, offset=0x%04X, main=0x%04X\n",
                source_path, info.bytes.size(), info.offset, info.mainEntry);
    std::printf("[smoke-source] %zu source lines, %zu addr lines, %zu entries, %zu subs, %zu statics, %zu aliases\n",
                info.sourceLines.size(), info.addrToLine.size(),
                info.entries.size(), info.subroutines.size(),
                info.statics.size(), info.aliases.size());

    FridgeCtx ctx;
    FridgeCtx_Init(ctx);

    FRIDGE_RAM_ADDR load_off = info.offset;
    if (info.bytes.size() > (size_t)(FRIDGE_RAM_SIZE - load_off))
    {
        std::fprintf(stderr, "[smoke-source] binary too large\n");
        return 1;
    }

    std::memcpy(ctx.cpu.ram + load_off, info.bytes.data(), info.bytes.size());
    ctx.cpu.PC = load_off;
    ctx.cpu.state = FRIDGE_CPU_ACTIVE;

    std::printf("[smoke-source] PC=%04X  A=%02X  state=%d  PANIC=%d\n",
                ctx.cpu.PC, ctx.cpu.rA, (int)ctx.cpu.state,
                (int)FRIDGE_cpu_flag_PANIC(&ctx.cpu));

    int steps = 0;
    const int max_steps = 1000000;
    while (ctx.cpu.state == FRIDGE_CPU_ACTIVE && steps < max_steps)
    {
        FRIDGE_sys_tick(&ctx.system);
        ++steps;
    }

    std::printf("[smoke-source] halted after %d steps: PC=%04X  A=%02X  state=%d  PANIC=%d\n",
                steps, ctx.cpu.PC, ctx.cpu.rA, (int)ctx.cpu.state,
                (int)FRIDGE_cpu_flag_PANIC(&ctx.cpu));

    unsigned non_zero = 0;
    for (int i = 0; i < FRIDGE_GPU_FRAME_BUFFER_SIZE; ++i)
        if (ctx.gpu.frame_a[i] != 0) ++non_zero;
    std::printf("[smoke-source] frame_a non-zero bytes: %u / %d\n",
                non_zero, FRIDGE_GPU_FRAME_BUFFER_SIZE);

    FridgeCtx_Shutdown(ctx);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc >= 3 && std::strcmp(argv[1], "--smoke-test") == 0)
        return SmokeTest(argv[2]);
    if (argc >= 3 && std::strcmp(argv[1], "--smoke-test-source") == 0)
        return SmokeTestSource(argv[2]);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        std::fprintf(stderr, "Error: %s\n", SDL_GetError());
        return 1;
    }

#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    float main_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN
                                                    | SDL_WINDOW_RESIZABLE
                                                    | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Fridge Emulator",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          (int)(1280 * main_scale), (int)(800 * main_scale),
                                          window_flags);
    if (window == nullptr)
    {
        std::fprintf(stderr, "Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }

    ImVector<const char*> extensions;
    uint32_t extensions_count = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, nullptr);
    extensions.resize(extensions_count);
    SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, extensions.Data);
    SetupVulkan(extensions);

    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface(window, g_Instance, &surface) == 0)
    {
        std::fprintf(stderr, "Failed to create Vulkan surface.\n");
        return 1;
    }

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    SetupVulkanWindow(wd, surface, w, h);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplSDL2_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.Allocator = g_Allocator;
    init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    App app;
    App_Init(app);

    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT
                && event.window.event == SDL_WINDOWEVENT_CLOSE
                && event.window.windowID == SDL_GetWindowID(window))
                done = true;

            // Forward keyboard events to the Fridge keyboard controller,
            // but skip when ImGui wants the keyboard (e.g. user editing hex
            // values in the register panel).
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
            {
                if (!ImGui::GetIO().WantCaptureKeyboard)
                    App_HandleSdlEvent(app, event);
            }
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        App_PumpPending(app);

        int fb_width, fb_height;
        SDL_GetWindowSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0
            && (g_SwapChainRebuild
                || g_MainWindowData.Width != fb_width
                || g_MainWindowData.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device,
                                                   wd, g_QueueFamily, g_Allocator,
                                                   fb_width, fb_height, g_MinImageCount, 0);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        App_DrawFrame(app);

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f
                                || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized)
        {
            wd->ClearValue.color.float32[0] = app.clear_color.x * app.clear_color.w;
            wd->ClearValue.color.float32[1] = app.clear_color.y * app.clear_color.w;
            wd->ClearValue.color.float32[2] = app.clear_color.z * app.clear_color.w;
            wd->ClearValue.color.float32[3] = app.clear_color.w;
            FrameRender(wd, draw_data);
            FramePresent(wd);
        }
    }

    VkResult err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    App_Shutdown(app);

    CleanupVulkanWindow(&g_MainWindowData);
    CleanupVulkan();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}