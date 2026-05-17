#include "stdafx.h"
#include "app_window.h"
#include "core_log.h"
//=============================================================================
namespace
{
	constexpr const wchar_t* ClassName = L"SEGameWindow";

	HWND      hwnd{ nullptr };
	HINSTANCE instance{ nullptr };

	uint16_t  windowWidth{ 0 };
	uint16_t  windowHeight{ 0 };
	float     windowAspect{ 1.0f };
	bool      windowClose{ true };

	bool      resizing{ false };

	MSG       msg = {};
}
//=============================================================================
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//=============================================================================
static void windowSetSize(uint16_t w, uint16_t h) noexcept
{
	windowWidth = std::max(w, static_cast<uint16_t>(1));
	windowHeight = std::max(h, static_cast<uint16_t>(1));
	windowAspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
}
//=============================================================================
uint16_t app::window::GetWidth() noexcept
{
	return windowWidth;
}
//=============================================================================
uint16_t app::window::GetHeight() noexcept
{
	return windowHeight;
}
//=============================================================================
float app::window::GetWindowAspect() noexcept
{
	return windowAspect;
}
//=============================================================================
// Main message handler for the sample.
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return 0;

	switch (message)
	{
	case WM_CLOSE:
		windowClose = true;
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_ENTERSIZEMOVE:
		resizing = true;
		return 0;
	case WM_EXITSIZEMOVE:
		resizing = false;
		return 0;
	case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
			minMaxInfo->ptMinTrackSize.x = 640;
			minMaxInfo->ptMinTrackSize.y = 360;
		}
		return 0;
	case WM_SIZE:
		if ((wParam != SIZE_MINIMIZED))
		{
			if ((resizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
			{
				windowSetSize(static_cast<uint16_t>(LOWORD(lParam)), static_cast<uint16_t>(HIWORD(lParam)));
			}
		}
		return 0;
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hWnd, message, wParam, lParam);
}
//=============================================================================
bool app::window::Init(const WindowCreateInfo& createInfo)
{
	resizing = false;

	instance = GetModuleHandle(nullptr);

	// Make process DPI aware and obtain main monitor scale
	ImGui_ImplWin32_EnableDpiAwareness();
	float mainScale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

	// Initialize the window class.
	WNDCLASSEX windowClass    = { 0 };
	windowClass.cbSize        = sizeof(WNDCLASSEX);
	windowClass.style         = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc   = WindowProc;
	windowClass.hInstance     = instance;
	windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = ClassName;
	if (!RegisterClassEx(&windowClass))
	{
		core::Error("Failed to register window class");
		return false;
	}

	RECT windowRect = { 0, 0, 
		static_cast<LONG>((float)createInfo.width * mainScale), 
		static_cast<LONG>((float)createInfo.height * mainScale) };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	// Create the window and store a handle to it.
	hwnd = CreateWindow(ClassName, createInfo.title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr, instance, nullptr);
	if (!hwnd)
	{
		core::Error("Failed to create window");
		return false;
	}
		
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);
	windowSetSize(
		static_cast<uint16_t>(clientRect.right - clientRect.left),
		static_cast<uint16_t>(clientRect.bottom - clientRect.top));

	// Show the window
	ShowWindow(hwnd, SW_SHOWDEFAULT);

	windowClose = false;
	return true;
}
//=============================================================================
void app::window::Close()
{
	if (hwnd) DestroyWindow(hwnd);
	if (instance) UnregisterClass(ClassName, instance);
	hwnd = nullptr;
	windowClose = true;
}
//=============================================================================
bool app::window::IsShouldClose() noexcept
{
	return windowClose;
}
//=============================================================================
void app::window::PollEvents()
{
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (msg.message == WM_QUIT)
		{
			windowClose = true;
			return;
		}
	}
}
//=============================================================================
HWND app::window::GetHwnd()
{
	return hwnd;
}
//=============================================================================
HINSTANCE app::window::GetInstance()
{
	return instance;
}
//=============================================================================