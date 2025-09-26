#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP

#include <string>

class FileUtils {
public:
    // MIME À… äQ
    static std::string getMimeType(const std::string& extension);
    static std::string getMimeTypeFromPath(const std::string& filepath);

    // Path traversal )À
    static bool isPathSecure(const std::string& path);
    static std::string normalizePath(const std::string& path);

    // | Œ\  ü1 ´l
    static bool hasPermission(const std::string& path);
    static bool fileExists(const std::string& path);
    static bool isDirectory(const std::string& path);
    static bool isReadable(const std::string& path);
    static bool isWritable(const std::string& path);

    // | l0
    static size_t getFileSize(const std::string& path);

    // U¥ ”œ
    static std::string getExtension(const std::string& path);

private:
    // İ1/Œx private ( ø¬ğ t˜¤)
    FileUtils();
    ~FileUtils();

    // õ¬ İ1@ `ù ğ° À
    FileUtils(const FileUtils&);
    FileUtils& operator=(const FileUtils&);

    // ´€  ø¬ğ hä
    static std::string toLowerCase(const std::string& str);
    static bool containsDotDot(const std::string& path);
    static bool isAbsolute(const std::string& path);
};

#endif // FILE_UTILS_HPP