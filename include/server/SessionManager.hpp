#ifndef SESSION_MANAGER_HPP
# define SESSION_MANAGER_HPP

#include <string>
#include <map>
#include <ctime>

/**
 * SessionManager - 쿠키/세션 관리 클래스
 *
 * 기능:
 * - 세션 생성/조회/삭제
 * - 파일 기반 세션 저장 (/tmp/webserv_sessions/)
 * - 사용자 정보 관리 (/tmp/webserv_users/)
 * - 30분 세션 만료 처리
 */
class SessionManager {
private:
	static const int SESSION_TIMEOUT = 1800; // 30분 (초 단위)
	static const std::string SESSION_DIR;     // "/tmp/webserv_sessions/"
	static const std::string USER_DIR;        // "/tmp/webserv_users/"

	struct Session {
		std::string sessionId;
		std::string username;
		time_t createdAt;
		time_t lastAccessedAt;

		Session() : sessionId(""), username(""), createdAt(0), lastAccessedAt(0) {}
		Session(const std::string& id, const std::string& user)
			: sessionId(id), username(user), createdAt(time(NULL)), lastAccessedAt(time(NULL)) {}
	};

	std::map<std::string, Session> _sessions; // sessionId -> Session

	// 내부 헬퍼 함수
	std::string	generateSessionId() const;
	bool		isSessionExpired(const Session& session) const;
	void		createDirectories();
	std::string	getSessionFilePath(const std::string& sessionId) const;
	std::string	getUserFilePath(const std::string& username) const;

public:
	SessionManager();
	~SessionManager();

	// 세션 관리
	std::string	createSession(const std::string& username);
	std::string	getUsername(const std::string& sessionId);
	bool		isValidSession(const std::string& sessionId);
	void		destroySession(const std::string& sessionId);
	void		updateLastAccessed(const std::string& sessionId);

	// 사용자 관리 (간단한 파일 기반)
	bool		registerUser(const std::string& username, const std::string& password);
	bool		authenticateUser(const std::string& username, const std::string& password);
	bool		userExists(const std::string& username);

	// 영속성
	void		loadSessions();
	void		saveSessions();

	// 유틸리티
	void		cleanupExpiredSessions();
};

#endif // SESSION_MANAGER_HPP
