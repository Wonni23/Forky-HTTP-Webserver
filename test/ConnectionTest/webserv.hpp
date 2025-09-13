#ifndef WEBSERV_HPP
# define WEBSERV_HPP

// C++ 98 Standard Libraries
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <exception>
#include <cstring>

// System Headers
#include <unistd.h>			// close, read, write
#include <sys/socket.h>		// Socket-related functions
#include <fcntl.h>			// File control (non-blocking mode set)
#include <errno.h>			// Error code checking
#include <sys/epoll.h>		// EventLoop.hpp
#include <time.h>			// time_t, time() for timeout handling

// Project Common Header
#include "Common.hpp"

#endif