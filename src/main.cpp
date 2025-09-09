#include "webserv.hpp"
#include "server/Server.hpp"
#include "config/ConfigParser.hpp"

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
		std::cout << "Starting Webserver..." << std::endl;

		// Server 객체가 main 함수 종료 시 자동으로 소멸자가 호출되어 리소스가 정리됩니다.
		Server webserv;

		// 설정 파일 파싱 로직 추가
		// ConfigParser parser(config_file);

		if (!webserv.init()) {
			std::cerr << "Error: Server initialization failed." << std::endl;
			return 1;
		}

		webserv.run();

	} catch (const std::exception& e) {
		std::cerr << "Server Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
