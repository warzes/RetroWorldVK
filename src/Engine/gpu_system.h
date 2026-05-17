#pragma once

namespace gpu
{
	struct CreateInfo final
	{
#if defined(_WIN32)
		HWND hwnd{ nullptr };
		HINSTANCE instance{ nullptr };
#endif
		bool vSync{ false };
	};

	bool Init(const CreateInfo& createInfo);
	void Close();

	void BeginFrame();
	void EndFrame();
} // namespace gpu