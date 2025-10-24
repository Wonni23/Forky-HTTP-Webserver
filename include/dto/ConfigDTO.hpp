#ifndef CONFIG_DTO_HPP
#define CONFIG_DTO_HPP

#include <vector>
#include <set>
#include <map>
#include <string>
#include <cstdlib>

struct BodySizeDirective {
    std::string size;  // "100M", "2000M" 등

    BodySizeDirective(const std::string& s) : size(s) {}
};

struct ListenDirective {
    std::string address;    // "192.168.1.100:8080", "80" 등
    bool default_server;    // default_server 키워드 여부
    std::string host;
    int port;

    ListenDirective(const std::string& addr, bool is_default = false) 
        : address(addr), default_server(is_default) {}
};

struct ServerNameDirective {
    std::string name;  // "example.com" 등
    
    ServerNameDirective(const std::string& n) : name(n) {}
};

struct ReturnDirective {
    int code;           // 301, 302 등
    std::string url;    // 리다이렉트할 URL
    
    ReturnDirective(int c, const std::string& u) : code(c), url(u) {}
};

struct RootDirective {
    std::string path;   // "/usr/share/nginx/" 등

    RootDirective(const std::string& p) : path(p) {}
};

struct AliasDirective {
    std::string path;   // "/data/images/" 등

    AliasDirective(const std::string& p) : path(p) {}
};

struct AutoindexDirective {
    bool enabled;       // on/off

    AutoindexDirective(bool e) : enabled(e) {}
};

struct IndexDirective {
    std::string filename;  // "index.html" 등
    
    IndexDirective(const std::string& f) : filename(f) {}
};

struct CgiPassDirective {
    std::string path;
    
    CgiPassDirective(const std::string& s) : path(s) {}
};

struct ErrorPageDirective {
    std::map<int, std::string> errorPageMap;  // status code -> path mapping

    ErrorPageDirective() {}

    ErrorPageDirective(const std::vector<int>& codes, const std::string& p) {
        for (size_t i = 0; i < codes.size(); ++i) {
            errorPageMap[codes[i]] = p;
        }
    }
};

struct LimitExceptDirective {
    std::set<std::string> allowed_methods;  // {"GET", "HEAD"} 등 (중복 자동 제거)
    bool deny_all;                             // deny all 여부
    
    LimitExceptDirective() : deny_all(false) {}
};

struct LocationContext {
    std::string path;  // "/admin/", "/static/" 등

    // Optional directives (vector로 구현, 0개 또는 1개 요소)
    std::vector<BodySizeDirective> opBodySizeDirective;
    std::vector<LimitExceptDirective> opLimitExceptDirective;
    std::vector<ReturnDirective> opReturnDirective;
    std::vector<RootDirective> opRootDirective;
    std::vector<AliasDirective> opAliasDirective;
    std::vector<AutoindexDirective> opAutoindexDirective;
    std::vector<IndexDirective> opIndexDirective;
    std::vector<CgiPassDirective> opCgiPassDirective;
    std::vector<ErrorPageDirective> opErrorPageDirective;

    LocationContext(const std::string& p) : path(p) {}
};

struct ServerContext {
    std::vector<LocationContext> locationContexts;

    // Optional directives (vector로 구현, 0개 또는 1개 요소)
    std::vector<BodySizeDirective> opBodySizeDirective;
    std::vector<ListenDirective> opListenDirective;
    std::vector<ServerNameDirective> opServerNameDirective;
    std::vector<ReturnDirective> opReturnDirective;
    std::vector<RootDirective> opRootDirective;
    std::vector<AutoindexDirective> opAutoindexDirective;
    std::vector<IndexDirective> opIndexDirective;
    std::vector<ErrorPageDirective> opErrorPageDirective;
};

struct HttpContext {
    std::vector<ServerContext> serverContexts;

    // Optional directives (vector로 구현, 0개 또는 1개 요소)
    std::vector<BodySizeDirective> opBodySizeDirective;
    std::vector<RootDirective> opRootDirective;
    std::vector<IndexDirective> opIndexDirective;
    std::vector<ErrorPageDirective> opErrorPageDirective;
};

struct ConfigDTO {
    HttpContext httpContext;
};

#endif