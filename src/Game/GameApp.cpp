#include "stdafx.h"
//=============================================================================
bool GameInit()
{
	return true;
}
//=============================================================================
void GameClose()
{
}
//=============================================================================
void GameUpdate()
{
}
//=============================================================================
void GameFixedUpdate()
{
}
//=============================================================================
void GameRender()
{
}
//=============================================================================
void GameRenderUI()
{
	//ImGui::Begin("Hello, world!");
	//ImGui::Text("This is some useful text.");
	//ImGui::End();
}
//=============================================================================
void GameApp()
{
	app::AppCreateInfo createInfo{};
	createInfo.init_cb = GameInit;
	createInfo.close_cb = GameClose;
	createInfo.update_cb = GameUpdate;
	createInfo.fixedUpdate_cb = GameFixedUpdate;
	createInfo.render_cb = GameRender;
	createInfo.renderUi_cb = GameRenderUI;

	app::Run(createInfo);
}
//=============================================================================