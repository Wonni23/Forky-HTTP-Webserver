#include "utils/FileManager.hpp"
#include "utils/Common.hpp"
#include <fstream>
#include <sstream>
#include <ctime>   // for time()
#include <cstdlib> // for rand()
#include <cstdio>  // for remove()
#include <iostream>
#include <dirent.h>
#include <sys/stat.h>

// private 생성자, 소멸자 정의.
FileManager::FileManager() {}
FileManager::~FileManager() {}

std::string FileManager::readFile(const std::string& path) {
	std::ifstream file(path.c_str());
	if (!file.is_open()) {
		// ERROR_LOG("readFile: Failed to open file: " + path);
		return "";
	}

	std::stringstream buffer;
	buffer << file.rdbuf();

	if (file.bad()) { // 읽기 작업 중 심각한 오류 발생 여부 확인.
		ERROR_LOG("readFile: Error occurred while reading file: " + path);
		file.close();
		return "";
	}
	file.close();

	return buffer.str();
}

bool FileManager::saveFile(const std::string& path, const std::string& content) {
	std::ofstream file(path.c_str(), std::ios::out | std::ios::trunc);
	if (!file.is_open()) {
		ERROR_LOG("saveFile: Failed to open or create file: " + path);
		return false;
	}

	file << content;

	// 쓰기 작업 직후 상태를 바로 확인!
	if (!file.good()) {
		ERROR_LOG("saveFile: Failed to write content to file: " + path);
		file.close();
		return false;
	}
	file.close();

	return true;
}

bool FileManager::deleteFile(const std::string& path) {
	if (std::remove(path.c_str()) != 0) {
		ERROR_LOG("deleteFile: Failed to delete file: " + path);
		return false;
	}
	return true;
}

std::string FileManager::generateUploadFilePath(const std::string& uploadDir) {
	std::stringstream ss;
	ss << uploadDir;

	if (!uploadDir.empty() && uploadDir[uploadDir.length() - 1] != '/') {
		ss << "/";
	}

	// 1. 현재 시간을 이용해 기본적인 고유성 확보.
	long timestamp = static_cast<long>(std::time(NULL));
	ss << timestamp;

	// 2. 동시 요청 충돌을 막기 위해 간단한 난수 추가.
	// (프로그램 시작 시 srand(time(NULL)) 한번 호출 필요) -> main
	ss << "_" << std::rand(); 

	ss << ".upload";
	return ss.str();
}

std::string FileManager::generateDirectoryListing(const std::string& dirPath, const std::string& requestPath) {
	// 1. 디렉토리 내용 읽기
	std::vector<DirectoryEntry> entries = readDirectory(dirPath);
	if (entries.empty()) {
		ERROR_LOG("generateDirectoryListing: Failed to read directory or directory is empty: " + dirPath);
		return "";
	}

	// 2. HTML 생성
	return buildDirectoryHTML(entries, requestPath);
}

std::vector<FileManager::DirectoryEntry> FileManager::readDirectory(const std::string& dirPath) {
	std::vector<DirectoryEntry> entries;

	DIR* dir = opendir(dirPath.c_str());
	if (dir == NULL) {
		ERROR_LOG("readDirectory: opendir failed: " + dirPath);
		return entries;
	}

	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL) {
		std::string name = entry->d_name;

		// ".", ".." 제외
		if (name == "." || name == "..") continue;

		// 전체 경로 생성
		std::string fullPath = dirPath;
		if (!dirPath.empty() && dirPath[dirPath.length() - 1] != '/') {
			fullPath += "/";
		}
		fullPath += name;

		// stat으로 파일 정보 확인
		struct stat fileStat;
		if (stat(fullPath.c_str(), &fileStat) != 0) {
			continue; // stat 실패하면 무시
		}

		bool isDir = S_ISDIR(fileStat.st_mode);
		size_t size = isDir ? 0 : static_cast<size_t>(fileStat.st_size);

		entries.push_back(DirectoryEntry(name, isDir, size));
	}

	closedir(dir);
	return entries;
}

std::string FileManager::buildDirectoryHTML(const std::vector<DirectoryEntry>& entries, const std::string& requestPath) {
	std::stringstream html;

	html << "<!DOCTYPE html>\n";
	html << "<html>\n<head>\n";
	html << "<title>Index of " << requestPath << "</title>\n";
	html << "<style>body{font-family:Arial,sans-serif;margin:20px;} ";
	html << "a{text-decoration:none;color:#0066cc;} ";
	html << "a:hover{text-decoration:underline;}</style>\n";
	html << "</head>\n<body>\n";
	html << "<h1>Index of " << requestPath << "</h1>\n";
	html << "<hr>\n<ul>\n";

	// 상위 디렉토리 링크 (requestPath가 "/"가 아닐 경우)
	if (requestPath != "/" && requestPath != "") {
		html << "<li><a href=\"../\">../</a></li>\n";
	}

	// 각 항목 출력
	for (size_t i = 0; i < entries.size(); ++i) {
		const DirectoryEntry& entry = entries[i];
		std::string link = entry.name;
		if (entry.isDirectory) link += "/";

		html << "<li><a href=\"" << link << "\">" << entry.name;
		if (entry.isDirectory) html << "/";
		html << "</a>";

		if (!entry.isDirectory) {
			html << " (" << entry.size << " bytes)";
		}
		html << "</li>\n";
	}

	html << "</ul>\n<hr>\n";
	html << "<footer>webserv/1.0</footer>\n";
	html << "</body>\n</html>";

	return html.str();
}