#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP

#include <string>

class FileUtils {
public:
    // MIME type mapping
    static std::string getMimeType(const std::string& extension);
    static std::string getMimeTypeFromPath(const std::string& filepath);

    // Path traversal prevention
    static bool isPathSecure(const std::string& path);
    static std::string normalizePath(const std::string& path);

    // File permissions and accessibility check
    static bool hasPermission(const std::string& path);
    static bool pathExists(const std::string& path);
    static bool isDirectory(const std::string& path);
    static bool isReadable(const std::string& path);
    static bool isWritable(const std::string& path);

    // File size
    static size_t getFileSize(const std::string& path);

    // Extension extraction
    static std::string getExtension(const std::string& path);

private:
    // Constructor/destructor private (utility class)
    FileUtils();
    ~FileUtils();

    // Copy constructor and assignment operator forbidden
    FileUtils(const FileUtils&);
    FileUtils& operator=(const FileUtils&);

    // Internal utility functions
    static std::string toLowerCase(const std::string& str);
    static bool containsDotDot(const std::string& path);
    static bool isAbsolute(const std::string& path);
};

#endif // FILE_UTILS_HPP