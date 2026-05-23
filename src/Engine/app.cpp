#include "stdafx.h"
#include "app.h"
#include "_app_window.h"
#include "_app_input.h"
#include "gpu_system.h"
//=============================================================================
namespace
{
	bool IsRequestingExit{ true };
	bool drawFPS{ false };
	// timer
	std::chrono::high_resolution_clock::time_point previousTime;
	std::chrono::high_resolution_clock::time_point currentTime;
	float                 deltaTime{ 0.0 };
	float                 accumulator{ 0.0f };
	constexpr const int   maxFixedSteps{ 5 };
	constexpr const float fixedDeltaTime = 1.0f / 60.0f;
	// fps
	constexpr const float avgInterval{ 0.5f };
	unsigned              frameCounter{ 0 };
	double                timeCounter{ 0.0 };
	float                 framesPerSecond{ 0.0f };
}
//=============================================================================
app::MessageHandler* userMessageHandler{ nullptr };
//=============================================================================
static void DrawFPS()
{
	if (const ImGuiViewport* v = ImGui::GetMainViewport())
	{
		ImGui::SetNextWindowPos({ v->WorkPos.x + v->WorkSize.x - 15.0f, v->WorkPos.y + 15.0f }, ImGuiCond_Always, { 1.0f, 0.0f });
	}
	ImGui::SetNextWindowBgAlpha(0.30f);
	ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize("FPS : _______").x, 0));
	if (ImGui::Begin("##FPS", nullptr,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove))
	{
		ImGui::Text("FPS : %i", (int)framesPerSecond);
		ImGui::Text("Ms  : %.1f", framesPerSecond > 0 ? 1000.0 / framesPerSecond : 0);
	}
	ImGui::End();
}
//=============================================================================
static bool Init(const app::AppCreateInfo& info)
{
	userMessageHandler = info.userMessageHandler;
	IsRequestingExit = false;
	currentTime = previousTime = std::chrono::high_resolution_clock::now();
	accumulator = 0.0f;
	deltaTime = 0.0f;
	frameCounter = 0;
	timeCounter = 0.0;

	drawFPS = info.drawFPS;

	if (!window::Init(info.window))
		return false;
	input::Init();

	gpu::CreateInfo gpuCreateInfo{};
	gpuCreateInfo.hwnd     = window::GetHwnd();
	gpuCreateInfo.instance = window::GetInstance();
	if (!gpu::Init(gpuCreateInfo))
		return false;

	return true;
}
//=============================================================================
static void Close()
{
	gpu::Close();
	window::Close();
	IsRequestingExit = true;
}
//=============================================================================
static bool IsShouldClose() noexcept
{
	return IsRequestingExit || window::IsShouldClose();
}
//=============================================================================
static void TimerUpdate()
{
	// 1. Расчёт deltaTime
	currentTime = std::chrono::high_resolution_clock::now();
	deltaTime = std::chrono::duration<float>(currentTime - previousTime).count();
	previousTime = currentTime;
	// 2. Защита от экстремальных значений (лагов, паузы)
	constexpr float MAX_DELTA = 0.25f; // 250 мс - максимальный допустимый шаг
	if (deltaTime > MAX_DELTA) deltaTime = MAX_DELTA;
	// 3. Расчет fps
	{
		frameCounter++;
		timeCounter += deltaTime;
		if (timeCounter > avgInterval)
		{
			framesPerSecond = static_cast<float>(frameCounter) / avgInterval;
			frameCounter = 0;
			timeCounter = 0.0;
		}
	}
	// Накапливаем время для fixedUpdate
	accumulator += deltaTime;
}
//=============================================================================
static bool Update(const app::AppCreateInfo& info)
{
	TimerUpdate();

	input::Reset();
	if (!window::PollEvents())
		return false;

	if (info.update_cb) info.update_cb();

	return true;
}
//=============================================================================
static void FixedUpdate(const app::AppCreateInfo& info)
{
	if (info.fixedUpdate_cb) info.fixedUpdate_cb();
}
//=============================================================================
static void Frame(const app::AppCreateInfo& info)
{
	if (gpu::BeginFrame())
	{
		if (info.render_cb) info.render_cb();
		if (info.renderUi_cb) info.renderUi_cb();

		//if (drawFPS) DrawFPS();
		gpu::EndFrame();
	}
}
//=============================================================================
void app::Run(const app::AppCreateInfo& info)
{
	if (Init(info))
	{
		bool userInit = true;
		if (info.init_cb) userInit = info.init_cb();
		if (userInit)
		{
			while (!IsShouldClose())
			{
				if (Update(info))
				{
					int steps = 0;
					while (accumulator >= fixedDeltaTime && steps < maxFixedSteps)
					{
						FixedUpdate(info);
						accumulator -= fixedDeltaTime;
						steps++;
					}

					//if (!windowMinimized)
						Frame(info);
				}
			}
		}

		if (info.close_cb) info.close_cb();
	}
	Close();
}
//=============================================================================
void app::Exit() noexcept
{
	IsRequestingExit = true;
	if (userMessageHandler) userMessageHandler->OnRequestingExit();
}
//=============================================================================
float app::GetDeltaTime() noexcept
{
	return deltaTime;
}
//=============================================================================