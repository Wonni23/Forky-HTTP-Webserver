#include <iostream>

int main(int argc, char* argv[]) {
	// Simple argument check
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
		return 1;
	}

	std::string configFile = argv[1];
	// TODO: Parse config file and initialize server

	std::cout << "Starting webserv with config: " << configFile << std::endl;

	// TODO: Start server loop

	// TODO: Clean up and shutdown

	return 0;
}