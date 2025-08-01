#include "webserv.hpp"
#include "server/Server.hpp"
#include "config/ConfigParser.hpp"

int main(int argc, char* argv[])
{
	std::string	config_file = "config/default.conf";

	if (argc > 1) {
		config_file = argv[1];
	} else if (argc > 2) {
		std::cerr << "Error: Too many arguments" << std::endl;
		return (1);
	}

	try {
		std::cout << "Starting Webserver" << std::endl;


	} catch (const std::exception& e) {
		std::cerr << "Server Error: " << e.what() << std::endl;
		return (1);
	}

	return (0);
}
