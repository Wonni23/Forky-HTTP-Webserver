#ifndef HTTP_CONTROLLER_HPP
# define HTTP_CONTROLLER_HPP

#include <string>

// Forward declarations
class HttpRequest;
class HttpResponse;
class ServerContext;
class LocationContext;

class HttpController {
private:
	// --- 각 HTTP 메서드별 핸들러 ---
	static HttpResponse* handleGetRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf);
	static HttpResponse* handlePostRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf);
	static HttpResponse* handleDeleteRequest(const HttpRequest* request, const ServerContext* serverConf, const LocationContext* locConf);
	
	// --- 내부 헬퍼 함수들 ---
	static bool isMethodAllowed(const std::string& method, const LocationContext* locConf);
	static HttpResponse* handleRedirect(const LocationContext* locConf);
	static HttpResponse* serveStaticFile(const std::string& filePath, const LocationContext* locConf);
	static HttpResponse* serveDirectoryListing(const std::string& dirPath, const std::string& uri);
	static HttpResponse* executeCgi(const HttpRequest* request, const std::string& cgiPath, const std::string& resourcePath);

public:
	// --- 유일한 public 인터페이스 ---
	static HttpResponse* processRequest(const HttpRequest* request, int connectedPort);
};

#endif // HTTP_CONTROLLER_HPP

