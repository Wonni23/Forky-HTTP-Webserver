#ifndef COMMON_HPP
# define COMMON_HPP

// Global Constant
# define MAX_EVENTS 1024
# define BUFFER_SIZE 65536
# define CLIENT_TIMEOUT 60 // 60seconds
# define CGI_TIMEOUT 5 // 5seconds

#include <iostream>

// Utility Macro
#ifdef DEBUG
	#define DEBUG_LOG(msg) do { std::cout << "[DEBUG] " << msg << std::endl; } while(0)
#else
	#define DEBUG_LOG(msg) ((void)0)
#endif

#define ERROR_LOG(msg) std::cerr << "[ERROR] " << msg << std::endl

#define INFO_LOG(msg) std::cout << "[INFO] " << msg << std::endl

#endif