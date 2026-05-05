#pragma once

#include <string>

namespace RXNEngine {

	class FileDialogs
	{
	public:
		static std::string OpenFile(const char* filter);
		static std::string SaveFile(const char* filter);
	};

	class FileSystem
	{
	public:
		static inline std::string GetRelativePath(const std::string& path)
		{
			if (path.empty())
				return "";

			std::filesystem::path filePath(path);

			if (!filePath.is_absolute())
				return filePath.generic_string();

			std::filesystem::path currentPath = std::filesystem::current_path();
			return std::filesystem::relative(filePath, currentPath).generic_string();
		}
	};
}