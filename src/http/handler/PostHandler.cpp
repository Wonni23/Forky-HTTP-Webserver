#include "http/handler/PostHandler.hpp"
#include "http/StatusCode.hpp"
#include "utils/PathResolver.hpp"
#include "utils/FileManager.hpp"
#include "utils/Common.hpp"
#include "http/MultipartFormDataParser.hpp"
#include <sys/stat.h> // mkdir, stat
#include <string.h>   // strerror
#include <errno.h>    // errno

/**
 * @brief PostHandler.cpp 내에서만 사용되는 헬퍼 함수들을
 * 익명 네임스페이스(anonymous namespace)에 캡슐화합니다.
 */
namespace {

// 1. 요청에서 본문(body)을 추출합니다.
std::string extractBody(const HttpRequest* request) {
	std::string bodyContent;
	if (request->isBodyByReference()) {
		const char* bodyData = request->getBodyData();
		size_t bodyLen = request->getBodyLength();
		bodyContent = std::string(bodyData, bodyLen);
		DEBUG_LOG("[PostHandler] Body size (zero-copy): " << bodyLen << " bytes");
	} else {
		bodyContent = request->getBody();
		DEBUG_LOG("[PostHandler] Body size (copied): " << bodyContent.length() << " bytes");
	}
	return bodyContent;
}

// 7. 빈 본문 (200 OK) 응답을 생성합니다.
HttpResponse* createEmptyBodyResponse() {
	DEBUG_LOG("[PostHandler] Empty body - no file created");
	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::OK);
	response->setBody("<html><body><h1>200 OK</h1><p>Empty POST request</p></body></html>");
	response->setContentType("text/html; charset=utf-8");
	return response;
}

// 2. Raw POST 요청을 처리합니다. (파일 내용/경로 설정)
void handleRawBody(const std::string& bodyContent,
					 const std::string& uploadRoot,
					 std::string& outFileContent, // [out]
					 std::string& outFilePath) { // [out]
	DEBUG_LOG("[PostHandler] POST type: Raw (e.g., text/plain)");
	outFileContent = bodyContent;
	outFilePath = FileManager::generateUploadFilePath(uploadRoot);
	DEBUG_LOG("[PostHandler] Using random path for raw POST");
}

// 3. Multipart 요청을 파싱합니다.
FilePart parseMultipartBody(const std::string& bodyContent,
							  const std::string& contentType) {
	DEBUG_LOG("[PostHandler] POST type: multipart/form-data");
	std::string boundary = MultipartFormDataParser::getBoundary(contentType);
	
	if (boundary.empty()) {
		ERROR_LOG("[PostHandler] Invalid multipart request: no boundary");
		return FilePart(); // isValid = false
	}
	return MultipartFormDataParser::parse(bodyContent, boundary);
}

// 4. 업로드될 최종 파일 경로를 결정합니다.
std::string determineUploadPath(const std::string& uploadRoot,
								const std::string& filenameFromPart) {
	if (!filenameFromPart.empty()) {
		// (보안 주의: ".."나 "/"가 있는지 검사하는 것이 좋지만, 일단은 그냥 함)
		DEBUG_LOG("[PostHandler] Using filename from part: " << filenameFromPart);
		return uploadRoot + "/" + filenameFromPart;
	} else {
		DEBUG_LOG("[PostHandler] No filename, using random path");
		return FileManager::generateUploadFilePath(uploadRoot);
	}
}

// 5. 파일 저장을 위한 디렉터리가 존재하는지 확인하고, 없으면 생성합니다.
bool ensureUploadDirectory(const std::string& filePath) {
	std::string dir = filePath.substr(0, filePath.find_last_of("/"));
	struct stat st;
	if (::stat(dir.c_str(), &st) != 0) {
		if (::mkdir(dir.c_str(), 0755) == -1) {
			ERROR_LOG("[PostHandler] Failed to create directory: " << dir << " (errno: " << strerror(errno) << ")");
			return false;
		}
		DEBUG_LOG("[PostHandler] Directory created: " << dir);
	}
	return true;
}

// 6. 성공 (201 Created) 응답을 생성합니다.
HttpResponse* createSuccessResponse(const std::string& filePath) {
	HttpResponse* response = new HttpResponse();
	response->setStatus(StatusCode::CREATED);
	response->setHeader("Location", filePath);
	response->setBody("<html><body><h1>201 Created</h1><p>File uploaded to " + filePath + "</p></body></html>");
	response->setContentType("text/html; charset=utf-8");
	return response;
}

} // 네임스페이스 종료


HttpResponse* PostHandler::handle(const HttpRequest* request,
								  const ServerContext* serverConf,
								  const LocationContext* locConf) {
	if (!request || !serverConf || !locConf) {
		ERROR_LOG("[PostHandler] NULL parameter in POST handler");
		return new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
		);
	}

	DEBUG_LOG("[PostHandler] ===== Handling POST request =====");

	// 1. 본문 통합
	std::string bodyContent = extractBody(request);

	// 2. 빈 본문 처리
	if (bodyContent.empty()) {
		return createEmptyBodyResponse();
	}

	// 3. 업로드 루트 경로 확인
	std::string uploadRoot = PathResolver::resolvePath(serverConf, locConf, request->getUri());
	if (uploadRoot.empty()) {
		ERROR_LOG("[PostHandler] Failed to resolve upload path");
		return new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::FORBIDDEN, serverConf, locConf)
		);
	}

	// 4. 파일 내용(fileContent) 및 경로(filePath) 결정
	std::string filePath;
	std::string fileContent;
	std::string contentType = request->getHeader("Content-Type");

	if (contentType.rfind("multipart/form-data", 0) == 0) {
		FilePart file = parseMultipartBody(bodyContent, contentType);
		
		if (!file.isValid || file.content.empty()) {
			ERROR_LOG("[PostHandler] Invalid multipart request: no file part found");
			return new HttpResponse(
				HttpResponse::createErrorResponse(StatusCode::BAD_REQUEST, serverConf, locConf)
			);
		}
		fileContent = file.content;
		filePath = determineUploadPath(uploadRoot, file.filename);

	} else {
		handleRawBody(bodyContent, uploadRoot, fileContent, filePath);
	}

	// 5. 업로드 디렉터리 준비
	if (!ensureUploadDirectory(filePath)) {
		ERROR_LOG("[PostHandler] Failed to create directory for file: " << filePath);
		return new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
		);
	}

	// 6. 파일 저장
	if (!FileManager::saveFile(filePath, fileContent)) {
		ERROR_LOG("[PostHandler] Failed to save file: " << filePath);
		return new HttpResponse(
			HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, serverConf, locConf)
		);
	}

	// 7. 성공 응답 생성
	DEBUG_LOG("[PostHandler] File uploaded: " << filePath);
	return createSuccessResponse(filePath);
}