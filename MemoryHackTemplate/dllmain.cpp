// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <iostream>
#include <Windows.h>
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <d3d9.h>
#include <tchar.h>

// Data
static LPDIRECT3D9         g_pD3D = nullptr;
static LPDIRECT3DDEVICE9     g_pd3dDevice = nullptr;
static bool                g_DeviceLost = false;
static UINT                g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS g_d3dpp = {};
static HWND                g_targetHwnd = nullptr;
static RECT                g_targetRect = { 0, 0, 0, 0 };
static HWND                g_overlayHwnd = nullptr;
static bool                g_showOverlay = true; // Control visibility
static bool                g_isMenuOpen = false;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

DWORD WINAPI ImGuiThread(LPVOID lpParam);
DWORD WINAPI OverlayThread(LPVOID lpParam);

// Main code
DWORD WINAPI ImGuiThread(LPVOID lpParam)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Overlay", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, wc.lpszClassName, L"Dear ImGui DirectX9 Overlay", WS_POPUP, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    if (!hwnd)
    {
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    g_overlayHwnd = hwnd; // Store the handle to the overlay window

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window initially
    if (g_showOverlay)
    {
        ::ShowWindow(hwnd, SW_SHOWNOACTIVATE); // Use SW_SHOWNOACTIVATE to prevent initial focus
    }
    else
    {
        ::ShowWindow(hwnd, SW_HIDE);
    }
    ::UpdateWindow(hwnd);
    ::SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_COLORKEY); // Make the window transparent (color key)

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;        // Enable Gamepad Controls
    io.IniFilename = nullptr; // Disable ini file saving

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX9_Init(g_pd3dDevice);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // Make ImGui background transparent

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle lost D3D9 device
        if (g_DeviceLost)
        {
            HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST)
            {
                ::Sleep(10);
                continue;
            }
            if (hr == D3DERR_DEVICENOTRESET)
                ResetDevice();
            g_DeviceLost = false;
        }

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            g_d3dpp.BackBufferWidth = g_ResizeWidth;
            g_d3dpp.BackBufferHeight = g_ResizeHeight;
            g_ResizeWidth = g_ResizeHeight = 0;
            ResetDevice();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Set ImGui window position and size to match the target window
        ImGui::SetNextWindowPos(ImVec2((float)g_targetRect.left, (float)g_targetRect.top));
        ImGui::SetNextWindowSize(ImVec2((float)(g_targetRect.right - g_targetRect.left), (float)(g_targetRect.bottom - g_targetRect.top)));

        // Begin the ImGui window
        ImGui::Begin("Overlay", nullptr, (g_isMenuOpen ? 0 : ImGuiWindowFlags_NoInputs) | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

        // Add your ImGui menu elements here
        if (g_isMenuOpen)
        {
            ImGui::Text("Overlay Menu!");
            if (ImGui::Button("Click Me"))
            {
                std::cout << "Button Clicked!" << std::endl;
            }
            if (ImGui::Button("Close Menu"))
            {
                g_isMenuOpen = false;
                ::SetWindowPos(g_overlayHwnd, HWND_TOPMOST, g_targetRect.left, g_targetRect.top, g_targetRect.right - g_targetRect.left, g_targetRect.bottom - g_targetRect.top, SWP_NOACTIVATE);
                ::SetForegroundWindow(g_targetHwnd); // Focus the game again
            }
            ImGui::Text("Target Window Dimensions: %d x %d", g_targetRect.right - g_targetRect.left, g_targetRect.bottom - g_targetRect.top);

            if (show_demo_window)
                ImGui::ShowDemoWindow(&show_demo_window);

            if (show_another_window)
            {
                ImGui::Begin("Another Window", &show_another_window);
                ImGui::Text("Hello from another window!");
                if (ImGui::Button("Close Me"))
                    show_another_window = false;
                ImGui::End();
            }
        }

        ImGui::End();

        // Rendering
        ImGui::EndFrame();
        g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(clear_color.x * clear_color.w * 255.0f), (int)(clear_color.y * clear_color.w * 255.0f), (int)(clear_color.z * clear_color.w * 255.0f), (int)(clear_color.w * 255.0f));
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
        if (result == D3DERR_DEVICELOST)
            g_DeviceLost = true;

        ::Sleep(1); // Small delay to reduce CPU usage
    }

    // Cleanup
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    // Create the D3DDevice
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;        // Present with vsync
    //g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;    // Present without vsync, maximum unthrottled framerate
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D()
{
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice()
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT(0);
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}


DWORD WINAPI Main(HMODULE hModule) {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    std::cout << "DLL Injected. Press END to detach." << std::endl;
    std::cout << "Press INSERT to toggle the overlay menu." << std::endl;

    // Find the target window
    g_targetHwnd = FindWindow(NULL, L"Skyrim Special Edition"); // Replace with the actual window name
    if (!g_targetHwnd) {
        std::cerr << "Error: Target window not found." << std::endl;
        if (f) fclose(f);
        FreeConsole();
        FreeLibraryAndExitThread(hModule, 1);
        return 1;
    }
    std::cout << "Target window found. HWND: " << g_targetHwnd << std::endl;

    CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)ImGuiThread, nullptr, 0, nullptr));

    while (true) {
        // Toggle menu visibility and focus
        if (GetAsyncKeyState(VK_INSERT) & 1) // Check if INSERT key was just pressed
        {
            g_showOverlay = !g_showOverlay;
            g_isMenuOpen = g_showOverlay; // Open menu when showing overlay

            if (g_overlayHwnd)
            {
                ::ShowWindow(g_overlayHwnd, g_showOverlay ? SW_SHOWNOACTIVATE : SW_HIDE);
                if (g_showOverlay)
                {
                    ::SetWindowPos(g_overlayHwnd, HWND_TOPMOST, g_targetRect.left, g_targetRect.top, g_targetRect.right - g_targetRect.left, g_targetRect.bottom - g_targetRect.top, SWP_NOACTIVATE);
                    ::SetForegroundWindow(g_overlayHwnd); // Focus the overlay for interaction
                }
                else
                {
                    ::SetForegroundWindow(g_targetHwnd); // Focus the game again when closing
                }
            }
        }

        // Update target window dimensions and position
        if (g_targetHwnd)
        {
            if (GetWindowRect(g_targetHwnd, &g_targetRect))
            {
                if (g_overlayHwnd && g_showOverlay)
                {
                    ::SetWindowPos(g_overlayHwnd, HWND_TOPMOST, g_targetRect.left, g_targetRect.top, g_targetRect.right - g_targetRect.left, g_targetRect.bottom - g_targetRect.top, SWP_NOACTIVATE);
                }
            }
        }

        if (GetAsyncKeyState(VK_END))
            break;
        ::Sleep(10);
    }

    if (f)
        fclose(f);
    FreeConsole();
    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Main, hModule, 0, nullptr));
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}