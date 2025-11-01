#ifndef FILE_MANAGER_HPP
#define FILE_MANAGER_HPP

#include <string>
#include <vector>

/**
 * @brief 파일 시스템 I/O 작업을 수행하는 정적 유틸리티 클래스.
 *
 * 파일의 생성, 읽기, 쓰기, 삭제 기능을 제공함.
 * 모든 함수는 static으로, 객체 생성 없이 사용 가능.
 */
class FileManager {
private:
	// 인스턴스화 방지를 위해 생성자와 소멸자를 private으로 선언.
	FileManager();
	~FileManager();

	/**
	 * @brief 디렉토리 항목(파일/폴더) 정보를 담는 구조체.
	 */
	struct DirectoryEntry {
		std::string name;     // 파일/폴더 이름
		bool isDirectory;     // 디렉토리 여부
		size_t size;          // 파일 크기 (디렉토리는 0)

		DirectoryEntry(const std::string& n, bool isDir, size_t s)
			: name(n), isDirectory(isDir), size(s) {}
	};

	/**
	 * @brief 디렉토리 내용 읽기. (비공개 헬퍼)
	 * @param dirPath 읽을 디렉토리 경로.
	 * @return 디렉토리 항목 벡터.
	 */
	static std::vector<DirectoryEntry> readDirectory(const std::string& dirPath);

	/**
	 * @brief HTML 생성. (비공개 헬퍼)
	 * @param entries 디렉토리 항목 벡터.
	 * @param requestPath HTTP 요청 경로.
	 * @return 생성된 HTML 문자열.
	 */
	static std::string buildDirectoryHTML(const std::vector<DirectoryEntry>& entries, const std::string& requestPath);


public:
	/**
	 * @brief 지정된 경로의 파일 내용을 전부 읽어 문자열로 반환.
	 * @param path 읽을 파일의 경로.
	 * @return 파일 내용을 담은 문자열. 실패 시 빈 문자열 반환.
	 */
	static bool readFile(const std::string& path, std::string& outContent);

	/**
	 * @brief 지정된 경로에 문자열 데이터를 저장.
	 * @param path 저장할 파일의 경로.
	 * @param content 저장할 문자열 데이터.
	 * @return 성공 시 true, 실패 시 false.
	 */
	static bool saveFile(const std::string& path, const std::string& content);

	/**
	 * @brief 지정된 경로의 파일을 삭제.
	 * @param path 삭제할 파일의 경로.
	 * @return 성공 시 true, 실패 시 false.
	 */
	static bool deleteFile(const std::string& path);

	/**
	 * @brief 파일 업로드를 위해 고유한 파일 경로를 생성.
	 * @param uploadDir 파일이 업로드될 디렉토리 경로.
	 * @return 생성된 고유 파일 경로.
	 */
	static std::string generateUploadFilePath(const std::string& uploadDir);

	/**
	 * @brief 디렉토리 리스팅 HTML 생성.
	 * @param dirPath 디렉토리 절대 경로 (예: "/var/www/uploads")
	 * @param requestPath HTTP 요청 경로 (예: "/uploads/") - HTML 내 링크 생성용
	 * @return 생성된 HTML 문자열. 실패 시 빈 문자열 반환.
	 */
	static std::string generateDirectoryListing(const std::string& dirPath, const std::string& requestPath);
};

#endif
