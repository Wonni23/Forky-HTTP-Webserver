#include "FileUtils.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

std::string FileUtils::getMimeType(const std::string& extension) {
    std::string ext = toLowerCase(extension);

    // HTML/CSS/JavaScript
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "json") return "application/json";

    // Images
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    if (ext == "bmp") return "image/bmp";
    if (ext == "webp") return "image/webp";

    // Text
    if (ext == "txt") return "text/plain";
    if (ext == "xml") return "application/xml";
    if (ext == "csv") return "text/csv";

    // Video
    if (ext == "mp4") return "video/mp4";
    if (ext == "avi") return "video/x-msvideo";
    if (ext == "mov") return "video/quicktime";
    if (ext == "wmv") return "video/x-ms-wmv";
    if (ext == "webm") return "video/webm";

    // Audio
    if (ext == "mp3") return "audio/mpeg";
    if (ext == "wav") return "audio/wav";
    if (ext == "ogg") return "audio/ogg";
    if (ext == "aac") return "audio/aac";

    // Documents
    if (ext == "pdf") return "application/pdf";
    if (ext == "doc") return "application/msword";
    if (ext == "docx") return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (ext == "xls") return "application/vnd.ms-excel";
    if (ext == "xlsx") return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    if (ext == "ppt") return "application/vnd.ms-powerpoint";
    if (ext == "pptx") return "application/vnd.openxmlformats-officedocument.presentationml.presentation";

    // Archives
    if (ext == "zip") return "application/zip";
    if (ext == "tar") return "application/x-tar";
    if (ext == "gz") return "application/gzip";
    if (ext == "rar") return "application/vnd.rar";
    if (ext == "7z") return "application/x-7z-compressed";

    // Fonts
    if (ext == "woff") return "font/woff";
    if (ext == "woff2") return "font/woff2";
    if (ext == "ttf") return "font/ttf";
    if (ext == "eot") return "application/vnd.ms-fontobject";

    // CGI scripts
    if (ext == "php") return "application/x-httpd-php";
    if (ext == "py") return "text/x-python";
    if (ext == "pl") return "text/x-perl";
    if (ext == "rb") return "text/x-ruby";

    return "application/octet-stream";
}

std::string FileUtils::getMimeTypeFromPath(const std::string& filepath) {
    std::string ext = getExtension(filepath);
    return getMimeType(ext);
}

bool FileUtils::isPathSecure(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    if (containsDotDot(path)) {
        return false;
    }

    std::string normalizedPath = normalizePath(path);

    if (normalizedPath.find('\0') != std::string::npos) {
        return false;
    }

    for (size_t i = 0; i < normalizedPath.length(); ++i) {
        unsigned char c = normalizedPath[i];
        if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
            return false;
        }
    }

    return true;
}

std::string FileUtils::normalizePath(const std::string& path) {
    if (path.empty()) {
        return "";
    }

    std::string result = path;

    for (size_t i = 0; i < result.length(); ++i) {
        if (result[i] == '\\') {
            result[i] = '/';
        }
    }

    std::string normalized;
    bool lastWasSlash = false;

    for (size_t i = 0; i < result.length(); ++i) {
        if (result[i] == '/') {
            if (!lastWasSlash) {
                normalized += '/';
                lastWasSlash = true;
            }
        } else {
            normalized += result[i];
            lastWasSlash = false;
        }
    }

    return normalized;
}

bool FileUtils::hasPermission(const std::string& path) {
    return access(path.c_str(), F_OK) == 0;
}

bool FileUtils::fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool FileUtils::isDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

bool FileUtils::isReadable(const std::string& path) {
    return access(path.c_str(), R_OK) == 0;
}

bool FileUtils::isWritable(const std::string& path) {
    return access(path.c_str(), W_OK) == 0;
}

size_t FileUtils::getFileSize(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return 0;
    }
    return static_cast<size_t>(st.st_size);
}

std::string FileUtils::getExtension(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    size_t slashPos = path.find_last_of('/');

    // No dot found
    if (dotPos == std::string::npos) {
        return "";
    }

    // Dot is before the last slash (part of directory name)
    if (slashPos != std::string::npos && dotPos < slashPos) {
        return "";
    }

    // Check if dot is at the beginning of filename (hidden file)
    size_t filenameStart = (slashPos == std::string::npos) ? 0 : slashPos + 1;
    if (dotPos == filenameStart) {
        return "";
    }

    return path.substr(dotPos + 1);
}

std::string FileUtils::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool FileUtils::containsDotDot(const std::string& path) {
    size_t pos = 0;
    while ((pos = path.find("..", pos)) != std::string::npos) {
        bool isStart = (pos == 0);
        bool isAfterSlash = (pos > 0 && path[pos - 1] == '/');
        bool isBeforeSlash = (pos + 2 < path.length() && path[pos + 2] == '/');
        bool isAtEnd = (pos + 2 == path.length());

        if ((isStart || isAfterSlash) && (isBeforeSlash || isAtEnd)) {
            return true;
        }

        pos += 2;
    }
    return false;
}

bool FileUtils::isAbsolute(const std::string& path) {
    return !path.empty() && path[0] == '/';
}