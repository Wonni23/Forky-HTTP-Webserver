#include "http/MultipartFormDataParser.hpp"
#include <iostream>

std::string MultipartFormDataParser::getBoundary(const std::string& contentTypeHeader) {
	size_t pos = contentTypeHeader.find("boundary=");
	if (pos == std::string::npos) {
		return "";
	}
	std::string boundary = contentTypeHeader.substr(pos + 9);
	// 가끔 boundary가 따옴표로 묶여있을 수 있음 (C++98이라 find_first_not_of 사용)
	size_t first = boundary.find_first_not_of(" \"");
	size_t last = boundary.find_last_not_of(" \"");
	if (first == std::string::npos || last == std::string::npos) {
		return boundary;
	}
	return boundary.substr(first, last - first + 1);
}

std::string MultipartFormDataParser::getFilename(const std::string& dispositionLine) {
	size_t pos = dispositionLine.find("filename=");
	if (pos == std::string::npos) {
		return "";
	}
	std::string filename = dispositionLine.substr(pos + 9);
	// 양쪽 따옴표 제거
	if (!filename.empty() && filename[0] == '"') {
		filename = filename.substr(1);
	}
	if (!filename.empty() && filename[filename.length() - 1] == '"') {
		filename = filename.erase(filename.length() - 1);
	}
	return filename;
}

FilePart MultipartFormDataParser::parse(const std::string& body, const std::string& boundary) {
	FilePart filePart;
	if (boundary.empty()) return filePart;

	std::string dashBoundary = "--" + boundary;
	std::string endBoundary = dashBoundary + "--";

	size_t startPos = body.find(dashBoundary);
	if (startPos == std::string::npos) return filePart;

	while (startPos != std::string::npos) {
		// 다음 경계선 찾기
		size_t endPos = body.find(dashBoundary, startPos + dashBoundary.length());
		if (endPos == std::string::npos) {
			// 마지막 경계선(endBoundary)인지 확인
			endPos = body.find(endBoundary, startPos + dashBoundary.length());
			if (endPos == std::string::npos) break; // 파싱 실패
		}

		// 현재 파트 추출 (경계선 포함)
		std::string part = body.substr(startPos, endPos - startPos);

		// 1. 파트 헤더 끝(CRLFCRLF) 찾기
		size_t headerEndPos = part.find("\r\n\r\n");
		if (headerEndPos == std::string::npos) {
			startPos = endPos; // 다음 파트로 이동
			continue;
		}
		
		// 2. 파트 헤더 추출
		std::string partHeaders = part.substr(0, headerEndPos);
		
		// 3. Content-Disposition에서 filename 찾기
		size_t dispPos = partHeaders.find("Content-Disposition:");
		if (dispPos != std::string::npos) {
			// Content-Disposition 라인의 끝(CRLF) 찾기
			size_t lineEndPos = partHeaders.find("\r\n", dispPos);
			std::string dispLine;
			if (lineEndPos == std::string::npos) {
				dispLine = partHeaders.substr(dispPos); // 마지막 줄일 경우
			} else {
				dispLine = partHeaders.substr(dispPos, lineEndPos - dispPos); // 해당 라인만 추출
			}

			std::string filename = getFilename(dispLine); // "한 줄"만 넘김
			
			if (!filename.empty()) {
				// 파일 찾음!
				filePart.filename = filename;
				
				// 4. 파일 내용 추출 (헤더 끝 + 4바이트, 뒤쪽 CRLF 2바이트 제거)
				size_t contentStart = headerEndPos + 4;
				size_t contentLength = (part.length() > contentStart + 2) ? (part.length() - contentStart - 2) : 0;
				filePart.content = part.substr(contentStart, contentLength);
				
				filePart.isValid = true;
				return filePart; // 첫 번째 파일만 찾고 반환
			}
		}
		
		startPos = endPos; // 다음 파트로 이동
	}

	return filePart; // 파일 못찾음
}