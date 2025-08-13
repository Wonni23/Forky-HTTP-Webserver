#ifndef CONFIG_DTO_HPP
#define CONFIG_DTO_HPP

#include <vector>

struct BodySizeDirective {

};

struct LocationContext {
    
};

struct ServerContext {
    std::vector<LocationContext> locationContexts;

    std::vector<BodySizeDirective> opBodySizeDirective;
    std::vector<ListenDirective> opListenDirective;
    std::vector<ServerNameDirective> opServerNameDirective;
    std::vector<ReturnDirective> opReturnDirective;
    std::vector<RootDirective> opRootDirective;
    std::vector<AutoindexDirective> opAutoindexDirective;
    std::vector<IndexDirective> 
};

struct HttpContext {
    std::vector<ServerContext> serverContexts;

    std::vector<BodySizeDirective> opBodySizeDirective;
    std::vector<RootDirective> opRootDirective;
    std::vector<IndexDirecrive> opIndexDirective;
    std::vector<ErrorPageDirective> opErrorPageDirective;
};

struct ConfigDTO {
    HttpContext httpContext;
};

#endif