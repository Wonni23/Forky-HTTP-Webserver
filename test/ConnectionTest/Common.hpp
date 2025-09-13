#ifndef COMMON_HPP
# define COMMON_HPP

// Global Constant
# define MAX_EVENTS 1024
# define BUFFER_SIZE 8192
# define MAX_REQUEST_SIZE (1024 * 1024) // 1MB
# define CLIENT_TIMEOUT 30 // 30seconds
# define CGI_TIMEOUT 30 // 30seconds
# define DEFAULT_ERRORPAGE_DIR "www/errorpages/"
# define DEFAULT_WEB_ROOT "www/"

// Utility Macro
#ifdef DEBUG
	#define DEBUG_LOG(msg) std::cout << "[DEBUG] " << msg << std::endl
#else
	#define DEBUG_LOG(msg)
#endif

#define ERROR_LOG(msg) std::cerr << "[ERROR] " << msg << std::endl

#define INFO_LOG(msg) std::cout << "[INFO] " << msg << std::endl

#endif