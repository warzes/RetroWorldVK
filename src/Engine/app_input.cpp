#include "stdafx.h"
#include "app_input.h"
#include "_app_input.h"
#include "app_messageHandler.h"
//=============================================================================
namespace
{
	bool         IsMouseMoveing{ false };
	float        MouseDelta{ 0.0f };
	math::point2 MouseLocation{ 0, 0 };

	// TODO: избавиться от мапы
	std::unordered_map<int, bool>         KeyActions;
	std::unordered_map<int, bool>         MouseActions;
	std::unordered_map<int, KeyboardType> KeyboardTypesMap;
}
//=============================================================================
extern app::MessageHandler* userMessageHandler;
//=============================================================================
void input::Init()
{
	KeyboardTypesMap[0x00B] = KeyboardType::KEY_0;
	KeyboardTypesMap[0x002] = KeyboardType::KEY_1;
	KeyboardTypesMap[0x003] = KeyboardType::KEY_2;
	KeyboardTypesMap[0x004] = KeyboardType::KEY_3;
	KeyboardTypesMap[0x005] = KeyboardType::KEY_4;
	KeyboardTypesMap[0x006] = KeyboardType::KEY_5;
	KeyboardTypesMap[0x007] = KeyboardType::KEY_6;
	KeyboardTypesMap[0x008] = KeyboardType::KEY_7;
	KeyboardTypesMap[0x009] = KeyboardType::KEY_8;
	KeyboardTypesMap[0x00A] = KeyboardType::KEY_9;
	KeyboardTypesMap[0x01E] = KeyboardType::KEY_A;
	KeyboardTypesMap[0x030] = KeyboardType::KEY_B;
	KeyboardTypesMap[0x02E] = KeyboardType::KEY_C;
	KeyboardTypesMap[0x020] = KeyboardType::KEY_D;
	KeyboardTypesMap[0x012] = KeyboardType::KEY_E;
	KeyboardTypesMap[0x021] = KeyboardType::KEY_F;
	KeyboardTypesMap[0x022] = KeyboardType::KEY_G;
	KeyboardTypesMap[0x023] = KeyboardType::KEY_H;
	KeyboardTypesMap[0x017] = KeyboardType::KEY_I;
	KeyboardTypesMap[0x024] = KeyboardType::KEY_J;
	KeyboardTypesMap[0x025] = KeyboardType::KEY_K;
	KeyboardTypesMap[0x026] = KeyboardType::KEY_L;
	KeyboardTypesMap[0x032] = KeyboardType::KEY_M;
	KeyboardTypesMap[0x031] = KeyboardType::KEY_N;
	KeyboardTypesMap[0x018] = KeyboardType::KEY_O;
	KeyboardTypesMap[0x019] = KeyboardType::KEY_P;
	KeyboardTypesMap[0x010] = KeyboardType::KEY_Q;
	KeyboardTypesMap[0x013] = KeyboardType::KEY_R;
	KeyboardTypesMap[0x01F] = KeyboardType::KEY_S;
	KeyboardTypesMap[0x014] = KeyboardType::KEY_T;
	KeyboardTypesMap[0x016] = KeyboardType::KEY_U;
	KeyboardTypesMap[0x02F] = KeyboardType::KEY_V;
	KeyboardTypesMap[0x011] = KeyboardType::KEY_W;
	KeyboardTypesMap[0x02D] = KeyboardType::KEY_X;
	KeyboardTypesMap[0x015] = KeyboardType::KEY_Y;
	KeyboardTypesMap[0x02C] = KeyboardType::KEY_Z;
	KeyboardTypesMap[0x028] = KeyboardType::KEY_APOSTROPHE;
	KeyboardTypesMap[0x02B] = KeyboardType::KEY_BACKSLASH;
	KeyboardTypesMap[0x033] = KeyboardType::KEY_COMMA;
	KeyboardTypesMap[0x00D] = KeyboardType::KEY_EQUAL;
	KeyboardTypesMap[0x029] = KeyboardType::KEY_GRAVE_ACCENT;
	KeyboardTypesMap[0x01A] = KeyboardType::KEY_LEFT_BRACKET;
	KeyboardTypesMap[0x00C] = KeyboardType::KEY_MINUS;
	KeyboardTypesMap[0x034] = KeyboardType::KEY_PERIOD;
	KeyboardTypesMap[0x01B] = KeyboardType::KEY_RIGHT_BRACKET;
	KeyboardTypesMap[0x027] = KeyboardType::KEY_SEMICOLON;
	KeyboardTypesMap[0x035] = KeyboardType::KEY_SLASH;
	KeyboardTypesMap[0x056] = KeyboardType::KEY_WORLD_2;
	KeyboardTypesMap[0x00E] = KeyboardType::KEY_BACKSPACE;
	KeyboardTypesMap[0x153] = KeyboardType::KEY_DELETE;
	KeyboardTypesMap[0x14F] = KeyboardType::KEY_END;
	KeyboardTypesMap[0x01C] = KeyboardType::KEY_ENTER;
	KeyboardTypesMap[0x001] = KeyboardType::KEY_ESCAPE;
	KeyboardTypesMap[0x147] = KeyboardType::KEY_HOME;
	KeyboardTypesMap[0x152] = KeyboardType::KEY_INSERT;
	KeyboardTypesMap[0x15D] = KeyboardType::KEY_MENU;
	KeyboardTypesMap[0x151] = KeyboardType::KEY_PAGE_DOWN;
	KeyboardTypesMap[0x149] = KeyboardType::KEY_PAGE_UP;
	KeyboardTypesMap[0x045] = KeyboardType::KEY_PAUSE;
	KeyboardTypesMap[0x146] = KeyboardType::KEY_PAUSE;
	KeyboardTypesMap[0x039] = KeyboardType::KEY_SPACE;
	KeyboardTypesMap[0x00F] = KeyboardType::KEY_TAB;
	KeyboardTypesMap[0x03A] = KeyboardType::KEY_CAPS_LOCK;
	KeyboardTypesMap[0x145] = KeyboardType::KEY_NUM_LOCK;
	KeyboardTypesMap[0x046] = KeyboardType::KEY_SCROLL_LOCK;
	KeyboardTypesMap[0x03B] = KeyboardType::KEY_F1;
	KeyboardTypesMap[0x03C] = KeyboardType::KEY_F2;
	KeyboardTypesMap[0x03D] = KeyboardType::KEY_F3;
	KeyboardTypesMap[0x03E] = KeyboardType::KEY_F4;
	KeyboardTypesMap[0x03F] = KeyboardType::KEY_F5;
	KeyboardTypesMap[0x040] = KeyboardType::KEY_F6;
	KeyboardTypesMap[0x041] = KeyboardType::KEY_F7;
	KeyboardTypesMap[0x042] = KeyboardType::KEY_F8;
	KeyboardTypesMap[0x043] = KeyboardType::KEY_F9;
	KeyboardTypesMap[0x044] = KeyboardType::KEY_F10;
	KeyboardTypesMap[0x057] = KeyboardType::KEY_F11;
	KeyboardTypesMap[0x058] = KeyboardType::KEY_F12;
	KeyboardTypesMap[0x064] = KeyboardType::KEY_F13;
	KeyboardTypesMap[0x065] = KeyboardType::KEY_F14;
	KeyboardTypesMap[0x066] = KeyboardType::KEY_F15;
	KeyboardTypesMap[0x067] = KeyboardType::KEY_F16;
	KeyboardTypesMap[0x068] = KeyboardType::KEY_F17;
	KeyboardTypesMap[0x069] = KeyboardType::KEY_F18;
	KeyboardTypesMap[0x06A] = KeyboardType::KEY_F19;
	KeyboardTypesMap[0x06B] = KeyboardType::KEY_F20;
	KeyboardTypesMap[0x06C] = KeyboardType::KEY_F21;
	KeyboardTypesMap[0x06D] = KeyboardType::KEY_F22;
	KeyboardTypesMap[0x06E] = KeyboardType::KEY_F23;
	KeyboardTypesMap[0x076] = KeyboardType::KEY_F24;
	KeyboardTypesMap[0x038] = KeyboardType::KEY_LEFT_ALT;
	KeyboardTypesMap[0x01D] = KeyboardType::KEY_LEFT_CONTROL;
	KeyboardTypesMap[0x02A] = KeyboardType::KEY_LEFT_SHIFT;
	KeyboardTypesMap[0x15B] = KeyboardType::KEY_LEFT_SUPER;
	KeyboardTypesMap[0x137] = KeyboardType::KEY_PRINT_SCREEN;
	KeyboardTypesMap[0x138] = KeyboardType::KEY_RIGHT_ALT;
	KeyboardTypesMap[0x11D] = KeyboardType::KEY_RIGHT_CONTROL;
	KeyboardTypesMap[0x036] = KeyboardType::KEY_RIGHT_SHIFT;
	KeyboardTypesMap[0x15C] = KeyboardType::KEY_RIGHT_SUPER;
	KeyboardTypesMap[0x150] = KeyboardType::KEY_DOWN;
	KeyboardTypesMap[0x14B] = KeyboardType::KEY_LEFT;
	KeyboardTypesMap[0x14D] = KeyboardType::KEY_RIGHT;
	KeyboardTypesMap[0x148] = KeyboardType::KEY_UP;
	KeyboardTypesMap[0x052] = KeyboardType::KEY_KP_0;
	KeyboardTypesMap[0x04F] = KeyboardType::KEY_KP_1;
	KeyboardTypesMap[0x050] = KeyboardType::KEY_KP_2;
	KeyboardTypesMap[0x051] = KeyboardType::KEY_KP_3;
	KeyboardTypesMap[0x04B] = KeyboardType::KEY_KP_4;
	KeyboardTypesMap[0x04C] = KeyboardType::KEY_KP_5;
	KeyboardTypesMap[0x04D] = KeyboardType::KEY_KP_6;
	KeyboardTypesMap[0x047] = KeyboardType::KEY_KP_7;
	KeyboardTypesMap[0x048] = KeyboardType::KEY_KP_8;
	KeyboardTypesMap[0x049] = KeyboardType::KEY_KP_9;
	KeyboardTypesMap[0x04E] = KeyboardType::KEY_KP_ADD;
	KeyboardTypesMap[0x053] = KeyboardType::KEY_KP_DECIMAL;
	KeyboardTypesMap[0x135] = KeyboardType::KEY_KP_DIVIDE;
	KeyboardTypesMap[0x11C] = KeyboardType::KEY_KP_ENTER;
	KeyboardTypesMap[0x037] = KeyboardType::KEY_KP_MULTIPLY;
	KeyboardTypesMap[0x04A] = KeyboardType::KEY_KP_SUBTRACT;
}
//=============================================================================
void input::Reset()
{
	MouseDelta = 0;
	IsMouseMoveing = false;
}
//=============================================================================
KeyboardType input::GetKeyFromKeyCode(int keyCode)
{
	auto it = KeyboardTypesMap.find(keyCode);
	if (it == KeyboardTypesMap.end())
		return KeyboardType::KEY_UNKNOWN;

	return it->second;
}
//=============================================================================
bool input::IsKeyDown(KeyboardType key)
{
	auto it = KeyActions.find((int)key);
	if (it == KeyActions.end())
		return false;

	return it->second == true;
}
//=============================================================================
bool input::IsKeyUp(KeyboardType key)
{
	auto it = KeyActions.find((int)key);
	if (it == KeyActions.end())
		return false;

	return it->second == false;
}
//=============================================================================
bool input::IsMouseDown(MouseType type)
{
	auto it = MouseActions.find((int)type);
	if (it == MouseActions.end())
	{
		return false;
	}
	return it->second == true;
}
//=============================================================================
bool input::IsMouseUp(MouseType type)
{
	auto it = MouseActions.find((int)type);
	if (it == MouseActions.end())
		return false;

	return it->second == false;
}
//=============================================================================
const math::point2& input::GetMousePosition()
{
	return MouseLocation;
}
//=============================================================================
float input::GetMouseDelta()
{
	return MouseDelta;
}
//=============================================================================
bool input::IsMouseMoving()
{
	return IsMouseMoveing;
}
//=============================================================================
void input::OnKeyDown(KeyboardType key)
{
	KeyActions[(int)key] = true;
	if (userMessageHandler) userMessageHandler->OnKeyDown(key);
}
//=============================================================================
void input::OnKeyUp(KeyboardType key)
{
	KeyActions[(int)key] = false;
	if (userMessageHandler) userMessageHandler->OnKeyUp(key);
}
//=============================================================================
void input::OnMouseDown(MouseType type, const math::point2& pos)
{
	MouseActions[(int)type] = true;
	MouseLocation = pos;
	if (userMessageHandler) userMessageHandler->OnMouseDown(type, pos);
}
//=============================================================================
void input::OnMouseUp(MouseType type, const math::point2& pos)
{
	MouseActions[(int)type] = false;
	MouseLocation = pos;
	if (userMessageHandler) userMessageHandler->OnMouseUp(type, pos);
}
//=============================================================================
void input::OnMouseMove(const math::point2& pos)
{
	IsMouseMoveing = true;
	MouseLocation = pos;
	if (userMessageHandler) userMessageHandler->OnMouseMove(pos);
}
//=============================================================================
void input::OnMouseWheel(const float delta, const math::point2& pos)
{
	MouseDelta = delta;
	MouseLocation = pos;
	if (userMessageHandler) userMessageHandler->OnMouseWheel(delta, pos);
}
//=============================================================================