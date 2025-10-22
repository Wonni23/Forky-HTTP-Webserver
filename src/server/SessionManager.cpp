#include "server/SessionManager.hpp"
#include "utils/Common.hpp"
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

// Static 멤버 초기화
const std::string SessionManager::SESSION_DIR = "/tmp/webserv_sessions/";
const std::string SessionManager::USER_DIR = "/tmp/webserv_users/";

SessionManager::SessionManager() {
	createDirectories();
	loadSessions();
	cleanupExpiredSessions();
}

SessionManager::~SessionManager() {
	saveSessions();
}

// ============================================================================
// 디렉토리 생성
// ============================================================================
void SessionManager::createDirectories() {
	// 세션 디렉토리 생성
	struct stat st;
	if (stat(SESSION_DIR.c_str(), &st) != 0) {
		mkdir(SESSION_DIR.c_str(), 0755);
		DEBUG_LOG("[SessionManager] Created session directory: " << SESSION_DIR);
	}

	// 사용자 디렉토리 생성
	if (stat(USER_DIR.c_str(), &st) != 0) {
		mkdir(USER_DIR.c_str(), 0755);
		DEBUG_LOG("[SessionManager] Created user directory: " << USER_DIR);
	}
}

// ============================================================================
// 세션 ID 생성 (UUID 스타일)
// ============================================================================
std::string SessionManager::generateSessionId() const {
	std::ostringstream oss;
	static int counter = 0;

	// 시간 + PID + 카운터 조합으로 유니크한 ID 생성
	oss << std::hex << time(NULL) << "-" << getpid() << "-" << counter++;

	return oss.str();
}

// ============================================================================
// 세션 만료 확인
// ============================================================================
bool SessionManager::isSessionExpired(const Session& session) const {
	time_t now = time(NULL);
	return (now - session.lastAccessedAt) > SESSION_TIMEOUT;
}

// ============================================================================
// 파일 경로 생성
// ============================================================================
std::string SessionManager::getSessionFilePath(const std::string& sessionId) const {
	return SESSION_DIR + sessionId + ".session";
}

std::string SessionManager::getUserFilePath(const std::string& username) const {
	return USER_DIR + username + ".user";
}

// ============================================================================
// 세션 생성
// ============================================================================
std::string SessionManager::createSession(const std::string& username) {
	std::string sessionId = generateSessionId();
	Session newSession(sessionId, username);

	_sessions[sessionId] = newSession;

	// 세션 파일 저장
	std::string filePath = getSessionFilePath(sessionId);
	std::ofstream ofs(filePath.c_str());
	if (ofs.is_open()) {
		ofs << "username=" << username << "\n";
		ofs << "created_at=" << newSession.createdAt << "\n";
		ofs << "last_accessed_at=" << newSession.lastAccessedAt << "\n";
		ofs.close();
		DEBUG_LOG("[SessionManager] Created session: " << sessionId << " for user: " << username);
	} else {
		ERROR_LOG("[SessionManager] Failed to create session file: " << filePath);
	}

	return sessionId;
}

// ============================================================================
// 세션에서 사용자 이름 조회
// ============================================================================
std::string SessionManager::getUsername(const std::string& sessionId) {
	std::map<std::string, Session>::iterator it = _sessions.find(sessionId);

	if (it == _sessions.end()) {
		return ""; // 세션 없음
	}

	Session& session = it->second;

	// 만료 확인
	if (isSessionExpired(session)) {
		DEBUG_LOG("[SessionManager] Session expired: " << sessionId);
		destroySession(sessionId);
		return "";
	}

	// 마지막 접근 시간 업데이트
	updateLastAccessed(sessionId);

	return session.username;
}

// ============================================================================
// 세션 유효성 확인
// ============================================================================
bool SessionManager::isValidSession(const std::string& sessionId) {
	return !getUsername(sessionId).empty();
}

// ============================================================================
// 세션 삭제
// ============================================================================
void SessionManager::destroySession(const std::string& sessionId) {
	std::map<std::string, Session>::iterator it = _sessions.find(sessionId);

	if (it != _sessions.end()) {
		_sessions.erase(it);

		// 세션 파일 삭제
		std::string filePath = getSessionFilePath(sessionId);
		if (unlink(filePath.c_str()) == 0) {
			DEBUG_LOG("[SessionManager] Destroyed session: " << sessionId);
		}
	}
}

// ============================================================================
// 마지막 접근 시간 업데이트
// ============================================================================
void SessionManager::updateLastAccessed(const std::string& sessionId) {
	std::map<std::string, Session>::iterator it = _sessions.find(sessionId);

	if (it != _sessions.end()) {
		it->second.lastAccessedAt = time(NULL);

		// 파일 업데이트
		std::string filePath = getSessionFilePath(sessionId);
		std::ofstream ofs(filePath.c_str());
		if (ofs.is_open()) {
			ofs << "username=" << it->second.username << "\n";
			ofs << "created_at=" << it->second.createdAt << "\n";
			ofs << "last_accessed_at=" << it->second.lastAccessedAt << "\n";
			ofs.close();
		}
	}
}

// ============================================================================
// 사용자 등록
// ============================================================================
bool SessionManager::registerUser(const std::string& username, const std::string& password) {
	if (userExists(username)) {
		ERROR_LOG("[SessionManager] User already exists: " << username);
		return false;
	}

	std::string filePath = getUserFilePath(username);
	std::ofstream ofs(filePath.c_str());

	if (!ofs.is_open()) {
		ERROR_LOG("[SessionManager] Failed to create user file: " << filePath);
		return false;
	}

	// 간단하게 평문 저장 (실제로는 해시 사용해야 하지만 과제 구색용)
	ofs << "username=" << username << "\n";
	ofs << "password=" << password << "\n";
	ofs.close();

	DEBUG_LOG("[SessionManager] Registered user: " << username);
	return true;
}

// ============================================================================
// 사용자 인증
// ============================================================================
bool SessionManager::authenticateUser(const std::string& username, const std::string& password) {
	std::string filePath = getUserFilePath(username);
	std::ifstream ifs(filePath.c_str());

	if (!ifs.is_open()) {
		ERROR_LOG("[SessionManager] User not found: " << username);
		return false;
	}

	std::string line;
	std::string storedPassword;

	while (std::getline(ifs, line)) {
		if (line.find("password=") == 0) {
			storedPassword = line.substr(9); // "password=" 길이
			break;
		}
	}
	ifs.close();

	bool authenticated = (storedPassword == password);

	if (authenticated) {
		DEBUG_LOG("[SessionManager] Authentication successful for user: " << username);
	} else {
		ERROR_LOG("[SessionManager] Authentication failed for user: " << username);
	}

	return authenticated;
}

// ============================================================================
// 사용자 존재 확인
// ============================================================================
bool SessionManager::userExists(const std::string& username) {
	std::string filePath = getUserFilePath(username);
	std::ifstream ifs(filePath.c_str());
	return ifs.good();
}

// ============================================================================
// 세션 로드 (서버 시작 시)
// ============================================================================
void SessionManager::loadSessions() {
	DIR* dir = opendir(SESSION_DIR.c_str());
	if (!dir) {
		ERROR_LOG("[SessionManager] Failed to open session directory: " << SESSION_DIR);
		return;
	}

	struct dirent* entry;
	int loadedCount = 0;

	while ((entry = readdir(dir)) != NULL) {
		std::string filename(entry->d_name);

		// .session 파일만 처리
		if (filename.length() > 8 && filename.substr(filename.length() - 8) == ".session") {
			std::string sessionId = filename.substr(0, filename.length() - 8);
			std::string filePath = getSessionFilePath(sessionId);

			std::ifstream ifs(filePath.c_str());
			if (!ifs.is_open()) continue;

			Session session;
			session.sessionId = sessionId;

			std::string line;
			while (std::getline(ifs, line)) {
				if (line.find("username=") == 0) {
					session.username = line.substr(9);
				} else if (line.find("created_at=") == 0) {
					session.createdAt = std::atol(line.substr(11).c_str());
				} else if (line.find("last_accessed_at=") == 0) {
					session.lastAccessedAt = std::atol(line.substr(17).c_str());
				}
			}
			ifs.close();

			// 만료되지 않은 세션만 로드
			if (!isSessionExpired(session)) {
				_sessions[sessionId] = session;
				loadedCount++;
			} else {
				// 만료된 세션 파일 삭제
				unlink(filePath.c_str());
			}
		}
	}
	closedir(dir);

	DEBUG_LOG("[SessionManager] Loaded " << loadedCount << " active sessions");
}

// ============================================================================
// 세션 저장 (서버 종료 시)
// ============================================================================
void SessionManager::saveSessions() {
	int savedCount = 0;

	for (std::map<std::string, Session>::iterator it = _sessions.begin();
	     it != _sessions.end(); ++it) {
		const Session& session = it->second;

		if (isSessionExpired(session)) {
			continue; // 만료된 세션은 저장하지 않음
		}

		std::string filePath = getSessionFilePath(session.sessionId);
		std::ofstream ofs(filePath.c_str());

		if (ofs.is_open()) {
			ofs << "username=" << session.username << "\n";
			ofs << "created_at=" << session.createdAt << "\n";
			ofs << "last_accessed_at=" << session.lastAccessedAt << "\n";
			ofs.close();
			savedCount++;
		}
	}

	DEBUG_LOG("[SessionManager] Saved " << savedCount << " active sessions");
}

// ============================================================================
// 만료된 세션 정리
// ============================================================================
void SessionManager::cleanupExpiredSessions() {
	std::vector<std::string> expiredSessions;

	// 만료된 세션 ID 수집
	for (std::map<std::string, Session>::iterator it = _sessions.begin();
	     it != _sessions.end(); ++it) {
		if (isSessionExpired(it->second)) {
			expiredSessions.push_back(it->first);
		}
	}

	// 만료된 세션 삭제
	for (size_t i = 0; i < expiredSessions.size(); ++i) {
		destroySession(expiredSessions[i]);
	}

	if (!expiredSessions.empty()) {
		DEBUG_LOG("[SessionManager] Cleaned up " << expiredSessions.size() << " expired sessions");
	}
}
