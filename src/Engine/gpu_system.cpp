#include "stdafx.h"
#include "gpu_system.h"
#include "app_window.h"
//=============================================================================
namespace
{
	bool       vSync{ false };
	VkExtent2D windowSize{ 0 };    // The window size
}
//=============================================================================
bool gpu::Init(const CreateInfo& createInfo)
{
	vSync = createInfo.vSync;
	windowSize.width = window::GetWidth();
	windowSize.height = window::GetHeight();

	return true;
}
//=============================================================================
void gpu::Close()
{
}
//=============================================================================
void gpu::BeginFrame()
{
	if (windowSize.width != window::GetWidth() || windowSize.height != window::GetHeight())
	{
		windowSize.width = window::GetWidth();
		windowSize.height = window::GetHeight();
	}
}
//=============================================================================
void gpu::EndFrame()
{
}
//=============================================================================