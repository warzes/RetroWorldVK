#pragma once

#include <3rdpartyConfig.h>

#if defined(_MSC_VER)
#	pragma warning(disable : 4820)
#	pragma warning(push, 3)
#	pragma warning(disable : 4865)
#	pragma warning(disable : 5039)
#endif

#define _USE_MATH_DEFINES

#if defined(_WIN32)

#	define WINVER 0x0A00 // windows 10
#	define _WIN32_WINNT	0x0A00 // windows 10

#	define NOGDICAPMASKS
#	define NOVIRTUALKEYCODES
//#	define NOWINMESSAGES
//#	define NOWINSTYLES
#	define NOSYSMETRICS
#	define NOMENUS
#	define NOICONS
#	define NOKEYSTATES
#	define NOSYSCOMMANDS
#	define NORASTEROPS
//#	define NOSHOWWINDOW
#	define OEMRESOURCE
#	define NOATOM
#	define NOCLIPBOARD
#	define NOCOLOR
#	define NOCTLMGR
#	define NODRAWTEXT
#	define NOGDI
#	define NOKERNEL
//#	define NOUSER
#	define NONLS
#	define NOMB
#	define NOMEMMGR
#	define NOMETAFILE
#	define NOMINMAX
//#	define NOMSG
#	define NOOPENFILE
#	define NOSCROLL
#	define NOSERVICE
#	define NOSOUND
#	define NOTEXTMETRIC
#	define NOWH
#	define NOWINOFFSETS
#	define NOCOMM
#	define NOKANJI
#	define NOHELP
#	define NOPROFILER
#	define NODEFERWINDOWPOS
#	define NOMCX
#	define WIN32_LEAN_AND_MEAN

#	include <Windows.h>
#endif // defined(_WIN32)

#include <cstdint>
#include <cassert>
#include <cmath>

#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <string>
#include <array>
#include <vector>
#include <span>
#include <unordered_map>

#include <volk/volk.h>
#include <vma/vk_mem_alloc.h>

#define USE_SLANG 0
#if USE_SLANG
#	include <slang/slang.h>
#	include <slang/slang-com-ptr.h>
#endif

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_vulkan.h>

#include <stb/stb_image.h>

#include <ktx/ktx.h>
#include <ktx/ktxvulkan.h>

#include <tiny_obj_loader/tiny_obj_loader.h>

#if defined(_MSC_VER)
#	pragma warning(pop)
#endif