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

	uint16_t GetWidth() noexcept;
	uint16_t GetHeight() noexcept;
	float GetWindowAspect() noexcept;
	bool GetWindowMinimized() noexcept;
} // namespace app::window