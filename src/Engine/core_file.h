#pragma once

namespace core
{
	bool Exists(const std::string& filePath);

	std::filesystem::path CurrentPath();

	std::string GetFileExtension(const std::string& filePath);
	std::string GetFileName(const std::string& filePath);
	std::string GetFileNameWithoutExtension(const std::string& filePath);
	std::string GetFileDirectory(const std::string& filePath);

	long long GetFileLastWriteTime(const std::string& filePath);

	void NormalizePathInline(std::string& file_path);
	std::string NormalizePath(const std::string& filePath);

	std::vector<char> ReadFile(const std::filesystem::path& path);
	bool SaveFile(const wchar_t* filePath, const void* data, size_t dataSize);
} // namespace core