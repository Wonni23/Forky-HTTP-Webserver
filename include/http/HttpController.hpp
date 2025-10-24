#ifndef HTTP_CONTROLLER_HPP
# define HTTP_CONTROLLER_HPP

#include <string>

// Forward declarations
class HttpRequest;
class HttpResponse;
struct ServerContext;
struct LocationContext;

class HttpController {
private:
	// --- CGI 관련 (TODO: 리팩토링 시 CgiService로 이동) ---
	static std::string		getCgiPath(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf);
	static HttpResponse*	executeCgi(const HttpRequest* request, const std::string& cgiPath, const ServerContext* serverConf, const LocationContext* locConf);

	// --- 각 HTTP 메서드별 핸들러 ---
	static HttpResponse*	handleGetRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf);
	static HttpResponse*	handlePostRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf);
	static HttpResponse*	handleDeleteRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf);

	// --- 내부 헬퍼 함수들 ---
	static HttpResponse*	handleRedirect(const LocationContext* locConf);
	static HttpResponse*	serveStaticFile(const std::string& filePath, const LocationContext* locConf);
	static HttpResponse*	serveDirectoryListing(const std::string& dirPath, const std::string& uri);

public:
	// --- 유일한 public 인터페이스 ---
	static HttpResponse*	processRequest(const HttpRequest* request, int connectedPort);
};

#endif // HTTP_CONTROLLER_HPP

