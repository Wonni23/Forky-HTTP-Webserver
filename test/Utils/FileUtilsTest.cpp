#include "FileUtils.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

class FileUtilsTest {
private:
    int totalTests;
    int passedTests;
    std::string testDir;

    void createTestFile(const std::string& filename, const std::string& content) {
        std::ofstream file((testDir + "/" + filename).c_str());
        file << content;
        file.close();
    }

    void createTestDir(const std::string& dirname) {
        mkdir((testDir + "/" + dirname).c_str(), 0755);
    }

    bool runTest(const std::string& testName, bool condition) {
        totalTests++;
        if (condition) {
            passedTests++;
            std::cout << "[PASS] " << testName << std::endl;
            return true;
        } else {
            std::cout << "[FAIL] " << testName << std::endl;
            return false;
        }
    }

public:
    FileUtilsTest() : totalTests(0), passedTests(0), testDir("test_files") {
        // Create test directory
        mkdir(testDir.c_str(), 0755);

        // Create test files and directories
        createTestFile("test.html", "<html><body>Test</body></html>");
        createTestFile("test.css", "body { color: red; }");
        createTestFile("test.js", "console.log('hello');");
        createTestFile("image.jpg", "fake jpg content");
        createTestFile("document.pdf", "fake pdf content");
        createTestFile("script.php", "<?php echo 'hello'; ?>");
        createTestFile("data.json", "{\"test\": true}");
        createTestFile("archive.zip", "fake zip content");
        createTestFile("noextension", "file without extension");
        createTestFile("empty.txt", "");

        createTestDir("subdir");
        createTestFile("subdir/nested.txt", "nested file");

        // Create files with special permissions
        createTestFile("readonly.txt", "readonly content");
        chmod((testDir + "/readonly.txt").c_str(), 0444);
    }

    ~FileUtilsTest() {
        // Cleanup test files
        system(("rm -rf " + testDir).c_str());
    }

    void testMimeTypes() {
        std::cout << "\n=== MIME Type Tests ===" << std::endl;

        runTest("HTML MIME type",
                FileUtils::getMimeType("html") == "text/html");
        runTest("CSS MIME type",
                FileUtils::getMimeType("css") == "text/css");
        runTest("JavaScript MIME type",
                FileUtils::getMimeType("js") == "application/javascript");
        runTest("JPEG MIME type",
                FileUtils::getMimeType("jpg") == "image/jpeg");
        runTest("PNG MIME type",
                FileUtils::getMimeType("png") == "image/png");
        runTest("PDF MIME type",
                FileUtils::getMimeType("pdf") == "application/pdf");
        runTest("PHP MIME type",
                FileUtils::getMimeType("php") == "application/x-httpd-php");
        runTest("Unknown extension",
                FileUtils::getMimeType("unknown") == "application/octet-stream");
        runTest("Case insensitive",
                FileUtils::getMimeType("HTML") == "text/html");

        // Test getMimeTypeFromPath
        runTest("Path to MIME - HTML",
                FileUtils::getMimeTypeFromPath("path/to/file.html") == "text/html");
        runTest("Path to MIME - No extension",
                FileUtils::getMimeTypeFromPath("path/to/file") == "application/octet-stream");
    }

    void testPathSecurity() {
        std::cout << "\n=== Path Security Tests ===" << std::endl;

        runTest("Safe path",
                FileUtils::isPathSecure("/safe/path/file.txt"));
        runTest("Reject path traversal ../",
                !FileUtils::isPathSecure("/path/../sensitive"));
        runTest("Reject path traversal /..",
                !FileUtils::isPathSecure("/path/.."));
        runTest("Reject path traversal at start",
                !FileUtils::isPathSecure("../sensitive"));
        runTest("Reject path traversal at end",
                !FileUtils::isPathSecure("/path/.."));
        runTest("Reject empty path",
                !FileUtils::isPathSecure(""));
        runTest("Allow dots in filename",
                FileUtils::isPathSecure("/path/file.name.txt"));
        runTest("Allow single dot",
                FileUtils::isPathSecure("/path/./file.txt"));
    }

    void testPathNormalization() {
        std::cout << "\n=== Path Normalization Tests ===" << std::endl;

        runTest("Normalize backslashes",
                FileUtils::normalizePath("path\\to\\file") == "path/to/file");
        runTest("Normalize double slashes",
                FileUtils::normalizePath("path//to//file") == "path/to/file");
        runTest("Normalize mixed slashes",
                FileUtils::normalizePath("path\\\\//to//\\file") == "path/to/file");
        runTest("Empty path normalization",
                FileUtils::normalizePath("") == "");
        runTest("Single slash",
                FileUtils::normalizePath("/") == "/");
        runTest("Multiple consecutive slashes",
                FileUtils::normalizePath("///path///") == "/path/");
    }

    void testFileOperations() {
        std::cout << "\n=== File Operations Tests ===" << std::endl;

        std::string htmlFile = testDir + "/test.html";
        std::string subdirPath = testDir + "/subdir";
        std::string nestedFile = testDir + "/subdir/nested.txt";
        std::string nonExistentFile = testDir + "/nonexistent.txt";
        std::string readonlyFile = testDir + "/readonly.txt";

        runTest("File exists - HTML",
                FileUtils::fileExists(htmlFile));
        runTest("File exists - nonexistent",
                !FileUtils::fileExists(nonExistentFile));

        runTest("Is directory - subdir",
                FileUtils::isDirectory(subdirPath));
        runTest("Is directory - file",
                !FileUtils::isDirectory(htmlFile));

        runTest("Is readable - HTML",
                FileUtils::isReadable(htmlFile));
        runTest("Is readable - nonexistent",
                !FileUtils::isReadable(nonExistentFile));

        runTest("Has permission - HTML",
                FileUtils::hasPermission(htmlFile));
        runTest("Has permission - nonexistent",
                !FileUtils::hasPermission(nonExistentFile));

        runTest("File size - HTML",
                FileUtils::getFileSize(htmlFile) > 0);
        runTest("File size - nonexistent",
                FileUtils::getFileSize(nonExistentFile) == 0);
        runTest("File size - empty",
                FileUtils::getFileSize(testDir + "/empty.txt") == 0);

        runTest("Is writable - normal file",
                FileUtils::isWritable(htmlFile));
        runTest("Is writable - readonly file",
                !FileUtils::isWritable(readonlyFile));
    }

    void testExtensionExtraction() {
        std::cout << "\n=== Extension Extraction Tests ===" << std::endl;

        runTest("Extract .html",
                FileUtils::getExtension("file.html") == "html");
        runTest("Extract .tar.gz",
                FileUtils::getExtension("archive.tar.gz") == "gz");
        runTest("No extension",
                FileUtils::getExtension("file") == "");
        runTest("Hidden file",
                FileUtils::getExtension(".hidden") == "");
        runTest("Extension in path",
                FileUtils::getExtension("path.dir/file.txt") == "txt");
        runTest("Dot at start of filename",
                FileUtils::getExtension("path/.config.json") == "json");
        runTest("Multiple dots",
                FileUtils::getExtension("file.name.backup.txt") == "txt");
    }

    void testEdgeCases() {
        std::cout << "\n=== Edge Cases Tests ===" << std::endl;

        // Test with very long paths
        std::string longPath(1000, 'a');
        longPath += ".txt";
        runTest("Long path MIME type",
                FileUtils::getMimeTypeFromPath(longPath) == "text/plain");

        // Test path security with complex paths
        runTest("Complex safe path",
                FileUtils::isPathSecure("/var/www/html/images/photo.jpg"));
        runTest("Tricky but safe path",
                FileUtils::isPathSecure("/path/file..txt"));

        // Test normalization edge cases
        runTest("Path with only slashes",
                FileUtils::normalizePath("////") == "/");
        runTest("Single character path",
                FileUtils::normalizePath("a") == "a");
    }

    void runAllTests() {
        std::cout << "Starting FileUtils Tests..." << std::endl;

        testMimeTypes();
        testPathSecurity();
        testPathNormalization();
        testFileOperations();
        testExtensionExtraction();
        testEdgeCases();

        std::cout << "\n=== Test Results ===" << std::endl;
        std::cout << "Passed: " << passedTests << "/" << totalTests << std::endl;

        if (passedTests == totalTests) {
            std::cout << "All tests passed! ✅" << std::endl;
        } else {
            std::cout << "Some tests failed! ❌" << std::endl;
        }
    }
};

int main() {
    FileUtilsTest test;
    test.runAllTests();
    return 0;
}