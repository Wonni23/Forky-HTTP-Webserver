#include "webserv.hpp"
#include "server/Server.hpp"
#include "config/ConfParser.hpp"
#include "config/ConfCascader.hpp"
#include "config/ConfApplicator.hpp"

int main(int argc, char* argv[]) {
	if (argc > 2) {
		std::cerr << "Error: Too many arguments. Usage: ./webserv [config_file]" << std::endl;
		return 1;
	}

	std::string config_file = "config/default.conf";
	if (argc == 2) {
		config_file = argv[1];
	}

	try {
		ConfParser	parser;
		ConfigDTO	config = parser.parseFile(config_file);

		ConfCascader	cascader;
		ConfigDTO	final_config = cascader.applyCascade(config);

		Server	webserv;
		if (!webserv.init()) {
			ERROR_LOG("Failed to initialize server");
			return 1;
		}

		ConfApplicator applicator;
		if (!applicator.applyConfig(&webserv, final_config)) {
			ERROR_LOG("Failed to apply configuration");
			return 1;
		}
		srand(time(NULL));

		INFO_LOG("Starting webserv...");
		webserv.run();
	} catch (const std::exception& e) {
		ERROR_LOG("Error: " << e.what());
		return 1;
	}

	return 0;
}
