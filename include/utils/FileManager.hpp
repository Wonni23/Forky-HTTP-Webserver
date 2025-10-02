#pragma once

#include <string>
#include "dto/ConfigDTO.hpp"

class FileManager {
public:
	static std::string readFile(const std::string& path);
	static bool saveFile(const std::string& path, const std::string& data);
	static bool deleteFile(const std::string& path);
	static std::string generateUploadFilePath(const std::string& uploadDir);
};
