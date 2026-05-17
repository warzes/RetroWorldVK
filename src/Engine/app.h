#pragma once

#include "app_window.h"

namespace app
{
	struct AppCreateInfo final
	{
		window::WindowCreateInfo window{};
		bool                     drawFPS{ true };

		bool (*init_cb)() = nullptr;
		void (*close_cb)() = nullptr;
		void (*update_cb)() = nullptr;
		void (*fixedUpdate_cb)() = nullptr;
		void (*render_cb)() = nullptr;
		void (*renderUi_cb)() = nullptr;
	};

	void Run(const AppCreateInfo& info);

	void Exit() noexcept;

	float GetDeltaTime() noexcept;
} // namespace app