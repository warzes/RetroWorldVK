#pragma once

namespace window
{
	struct WindowCreateInfo final
	{
		std::wstring title{ L"Game" };
		uint16_t width{ 1600 };
		uint16_t height{ 900 };
		bool fullscreen{ false };
	};

	bool Init(const WindowCreateInfo& createInfo);
	void Close();
	bool IsShouldClose() noexcept;
	bool PollEvents();

	HWND GetHwnd();
	HINSTANCE GetInstance();

	uint16_t GetWidth() noexcept;
	uint16_t GetHeight() noexcept;
	float GetWindowAspect() noexcept;
} // namespace app::window