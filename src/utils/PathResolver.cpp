#include "utils/PathResolver.hpp"
#include "ConfigDTO.hpp"
#include "FileUtils.hpp"

std::string PathResolver::resolvePath(const ServerContext* server, const LocationContext* loc, const std::string& uri) {
	if (server == NULL || loc == NULL) {
		return ""; // Cannot resolve path without server or location configuration.
	}

	// 1. Check for an 'alias' directive (highest priority).
	if (!loc->opAliasDirective.empty()) {
		const std::string& alias_path = loc->opAliasDirective[0].path;
		const std::string& location_path = loc->path;
		
		// Check if the request URI starts with the location path.
		if (uri.compare(0, location_path.length(), location_path) == 0) {
			// Cut off the location part and append the remainder to the alias path.
			std::string remainder = uri.substr(location_path.length());
			
			std::string resolved = alias_path;
			if (resolved.length() > 0 && resolved[resolved.length() - 1] != '/' && !remainder.empty() && remainder[0] != '/') {
				resolved += '/';
			}
			resolved += remainder;
			
			// Clean up the path and return.
			return FileUtils::normalizePath(resolved); // normalizePath cleans up items like double slashes.
		}
	}
	
	// 2. If no alias is found, process with the 'root' directive.
	std::string root_path;
	if (!loc->opRootDirective.empty()) {
		root_path = loc->opRootDirective[0].path;
	} else if (!server->opRootDirective.empty()) {
		root_path = server->opRootDirective[0].path;
	} else {
		root_path = "/var/www/html"; // Default value.
	}

	std::string resolved = root_path;
	if (resolved.length() > 0 && resolved[resolved.length() - 1] == '/' && uri.length() > 0 && uri[0] == '/') {
		// If root ends with '/' and the URI starts with '/', remove the duplicate slash.
		resolved += uri.substr(1);
	} else if (resolved.length() > 0 && resolved[resolved.length() - 1] != '/' && uri.length() > 0 && uri[0] != '/') {
		resolved += '/';
		resolved += uri;
	} else {
		resolved += uri;
	}

	return FileUtils::normalizePath(resolved);
}


std::string PathResolver::findIndexFile(const std::string& dirPath, const LocationContext* loc)
{

}

std::string PathResolver::getCgiInterpreter(const std::string& uri, const LocationContext* loc)
{

}

