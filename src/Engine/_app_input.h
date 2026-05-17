#pragma once

#include "app_input.h"

namespace input
{
	void Init();
	void Reset();

	KeyboardType GetKeyFromKeyCode(int keyCode);

	void OnKeyDown(KeyboardType key);
	void OnKeyUp(KeyboardType key);
	void OnMouseDown(MouseType type, const math::point2& pos);
	void OnMouseUp(MouseType type, const math::point2& pos);
	void OnMouseMove(const math::point2& pos);
	void OnMouseWheel(const float delta, const math::point2& pos);
} // namespace input