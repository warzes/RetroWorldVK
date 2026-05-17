#pragma once

#include "app_keys.h"
#include "math_point2.h"

namespace app
{
	class MessageHandler
	{
	public:
		MessageHandler() noexcept = default;
		virtual ~MessageHandler() = default;

		virtual bool OnKeyDown(const KeyboardType key) { return false; }
		virtual bool OnKeyUp(const KeyboardType key) { return false; }

		virtual bool OnMouseDown(MouseType type, const math::point2& pos) { return false; }
		virtual bool OnMouseUp(MouseType type, const math::point2& pos) { return false; }
		//virtual bool OnMouseDoubleClick(MouseType type, const math::point2& pos) { return false; }
		virtual bool OnMouseWheel(const float delta, const math::point2& pos) { return false; }
		virtual bool OnMouseMove(const math::point2& pos) { return false; }

		virtual bool OnSizeChanged(const uint16_t width, const uint16_t height) { return false; }
		//virtual void OnResizingWindow() { }

		//virtual void HandleDPIScaleChanged() {}
		//virtual void SignalSystemDPIChanged() {}

		//virtual void OnMovedWindow(const int x, const int y) {}
		virtual void OnWindowClose() {}

		virtual void OnRequestingExit() {}
	};
} // namespace app