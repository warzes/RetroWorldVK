#include "stdafx.h"
#include "core_file.h"
#include "core_log.h"
//=============================================================================
bool core::Exists(const std::string& filePath)
{
	return std::filesystem::exists(filePath) && std::filesystem::is_regular_file(filePath);
}
//=============================================================================
std::filesystem::path core::CurrentPath()
{
	return std::filesystem::current_path();
}
//=============================================================================
std::string core::GetFileExtension(const std::string& filePath)
{
	std::filesystem::path path(filePath);
	return path.extension().string();
}
//=============================================================================
std::string core::GetFileName(const std::string& filePath)
{
	std::filesystem::path path(filePath);
	return path.filename().string();
}
//=============================================================================
std::string core::GetFileNameWithoutExtension(const std::string& filePath)
{
	std::filesystem::path path(filePath);
	return path.filename().replace_extension().string();
}
//=============================================================================
std::string core::GetFileDirectory(const std::string& filePath)
{
	std::filesystem::path path(filePath);
	return path.parent_path().string() + "/";
}
//=============================================================================
long long core::GetFileLastWriteTime(const std::string& filePath)
{
	std::filesystem::path path(filePath);
	return std::filesystem::last_write_time(path).time_since_epoch().count();
}
//=============================================================================
void core::NormalizePathInline(std::string& filePath)
{
	for (char& c : filePath)
	{
		if (c == '\\')
		{
			c = '/';
		}
	}
	if (filePath.find("./") == 0)
	{
		filePath = std::string(filePath.begin() + 2, filePath.end());
	}
}
//=============================================================================
std::string core::NormalizePath(const std::string& filePath)
{
	std::string output = std::string(filePath.begin(), filePath.end());
	NormalizePathInline(output);
	return output;
}
//=============================================================================
std::vector<char> core::ReadFile(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		Error("Fail to open file: " + path.string());
		return {};
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> out(fileSize);

	file.seekg(0);
	file.read(out.data(), fileSize);
	file.close();
	return out;
}
//=============================================================================
bool core::SaveFile(const wchar_t* filePath, const void* data, size_t dataSize)
{
	FILE* f = nullptr;
	_wfopen_s(&f, filePath, L"wb");
	if (f)
	{
		fwrite(data, 1, dataSize, f);
		fclose(f);
		return true;
	}

	return false;
}
//=============================================================================