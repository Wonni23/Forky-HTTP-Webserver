#ifndef COMMON_HPP
# define COMMON_HPP

// Global Constant
# define MAX_EVENTS 1024
# define BUFFER_SIZE 8192
# define CLIENT_TIMEOUT 60 // 60seconds
# define CGI_TIMEOUT 60 // 60seconds

#include <iostream>

// Utility Macro
#ifdef DEBUG
	#define DEBUG_LOG(msg) std::cout << "[DEBUG] " << msg << std::endl
#else
	#define DEBUG_LOG(msg)
#endif

#define ERROR_LOG(msg) std::cerr << "[ERROR] " << msg << std::endl

#define INFO_LOG(msg) std::cout << "[INFO] " << msg << std::endl

#endif