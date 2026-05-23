#include "stdafx.h"
#include "app_window.h"
#include "_app_window.h"
#include "core_log.h"
#include "_app_input.h"
#include "app_messageHandler.h"
//=============================================================================
#ifndef GET_X_LPARAM
#	define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#	define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif
//=============================================================================
namespace
{
	constexpr const wchar_t* AppWindowClass = L"SEGameWindow";

	HWND      hwnd{ nullptr };
	HINSTANCE instance{ nullptr };

	uint16_t  windowWidth{ 0 };
	uint16_t  windowHeight{ 0 };
	float     windowAspect{ 1.0f };
	bool      windowClose{ true };

	bool      resizing{ false };
	bool      windowMinimized{ false };

	MSG       msg = {};
}
//=============================================================================
extern app::MessageHandler* userMessageHandler;
//=============================================================================
static void windowSetSize(uint16_t w, uint16_t h) noexcept
{
	windowWidth = std::max(w, static_cast<uint16_t>(1));
	windowHeight = std::max(h, static_cast<uint16_t>(1));
	windowAspect = static_cast<float>(windowWidth) / static_cast<float>(windowHeight);
}
//=============================================================================
uint16_t window::GetWidth() noexcept
{
	return windowWidth;
}
//=============================================================================
uint16_t window::GetHeight() noexcept
{
	return windowHeight;
}
//=============================================================================
float window::GetWindowAspect() noexcept
{
	return windowAspect;
}
//=============================================================================
bool window::GetWindowMinimized() noexcept
{
	return windowMinimized;
}
//=============================================================================
// Main message handler for the sample.
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
	extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return 0;

	switch (message)
	{
	case WM_CLOSE:
		windowClose = true;
		if (userMessageHandler) userMessageHandler->OnWindowClose();
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
		windowMinimized = (wParam == SIZE_MINIMIZED);
		if ((wParam != SIZE_MINIMIZED))
		{
			if ((resizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
			{
				uint16_t width = static_cast<uint16_t>(LOWORD(lParam));
				uint16_t height = static_cast<uint16_t>(HIWORD(lParam));
				windowSetSize(width, height);
				if (userMessageHandler) userMessageHandler->OnSizeChanged(width, height);
			}
		}
		return 0;
	case WM_KEYDOWN:
		{
			const int keycode = HIWORD(lParam) & 0x1FF;
			KeyboardType key = input::GetKeyFromKeyCode(keycode);
			input::OnKeyDown(key);
			return 0;
		}
	case WM_KEYUP:
		{
			const int keycode = HIWORD(lParam) & 0x1FF;
			KeyboardType key = input::GetKeyFromKeyCode(keycode);
			input::OnKeyUp(key);
			return 0;
		}

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
		{
			MouseType button = MouseType::MOUSE_BUTTON_LEFT;

			const int x = GET_X_LPARAM(lParam);
			const int y = GET_Y_LPARAM(lParam);
			math::point2 pos(x, y);

			if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP)
			{
				button = MouseType::MOUSE_BUTTON_LEFT;
			}
			else if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP)
			{
				button = MouseType::MOUSE_BUTTON_RIGHT;
			}
			else if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP)
			{
				button = MouseType::MOUSE_BUTTON_MIDDLE;
			}
			else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
			{
				button = MouseType::MOUSE_BUTTON_4;
			}
			else
			{
				button = MouseType::MOUSE_BUTTON_5;
			}

			if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN)
			{
				input::OnMouseDown(button, pos);
			}
			else
			{
				input::OnMouseUp(button, pos);
			}

			return 0;
		}

	case WM_MOUSEMOVE:
		{
			const int x = GET_X_LPARAM(lParam);
			const int y = GET_Y_LPARAM(lParam);
			math::point2 pos(x, y);
			input::OnMouseMove(pos);
			return 0;
		}
	case WM_MOUSEWHEEL:
		{
			const int x = GET_X_LPARAM(lParam);
			const int y = GET_Y_LPARAM(lParam);
			math::point2 pos((float)x, (float)y);
			input::OnMouseWheel((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, pos);
			return 0;
		}
	case WM_MOUSEHWHEEL:
		{
			const int x = GET_X_LPARAM(lParam);
			const int y = GET_Y_LPARAM(lParam);
			math::point2 pos((float)x, (float)y);
			input::OnMouseWheel((float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA, pos);
			return 0;
		}
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hWnd, message, wParam, lParam);
}
//=============================================================================
bool window::Init(const WindowCreateInfo& createInfo)
{
	resizing = false;
	windowClose = false;
	windowMinimized = false;

	instance = GetModuleHandle(nullptr);

	WNDCLASSEX windowClass    = { 0 };
	windowClass.cbSize        = sizeof(WNDCLASSEX);
	windowClass.style         = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc   = WindowProc;
	windowClass.hInstance     = instance;
	windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = AppWindowClass;
	if (!RegisterClassEx(&windowClass))
	{
		core::Fatal("Failed to register window class");
		return false;
	}

	RECT windowRect = { 0, 0, 
		static_cast<LONG>(createInfo.width), 
		static_cast<LONG>(createInfo.height) };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	hwnd = CreateWindow(AppWindowClass, createInfo.title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr, instance, nullptr);
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

	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);

	return true;
}
//=============================================================================
void window::Close()
{
	if (hwnd) DestroyWindow(hwnd);
	if (instance) UnregisterClass(AppWindowClass, instance);
	hwnd = nullptr;
	windowClose = true;
}
//=============================================================================
bool window::IsShouldClose() noexcept
{
	return windowClose;
}
//=============================================================================
bool window::PollEvents()
{
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (msg.message == WM_QUIT)
		{
			windowClose = true;
			return false;
		}
	}
	return true;
}
//=============================================================================
HWND window::GetHwnd()
{
	return hwnd;
}
//=============================================================================
HINSTANCE window::GetInstance()
{
	return instance;
}
//=============================================================================