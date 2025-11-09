// Server.cpp - 필수 로그만 포함
#include "server/Server.hpp"
#include "server/Client.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "http/HttpController.hpp"
#include "http/RequestRouter.hpp"
#include "http/StatusCode.hpp"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

// 생성자 및 소멸자
Server::Server(void)
	: _event_loop(NULL), _sessionManager(NULL), _running(false) {
	_event_loop = new EventLoop();
	_sessionManager = new SessionManager();
}

Server::~Server(void) {
	stop();
	if (_sessionManager) delete _sessionManager;
	if (_event_loop) delete _event_loop;
}

// Private Functions
int Server::createServerSocket(void) {
	int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		ERROR_LOG("[Server] socket() failed: " << std::strerror(errno));
		return -1;
	}

	int opt = 1;
	if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		ERROR_LOG("[Server] setsockopt failed");
		::close(fd);
		return -1;
	}

	if (::fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		ERROR_LOG("[Server] fcntl failed");
		::close(fd);
		return -1;
	}

	return fd;
}

bool Server::bindAndListen(int fd, const std::string& host, int port) {
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (host == "0.0.0.0" || host.empty()) {
		addr.sin_addr.s_addr = INADDR_ANY;
	} else {
		if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
			ERROR_LOG("[Server] invalid host: " << host);
			return false;
		}
	}

	if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		ERROR_LOG("[Server] bind failed: " << std::strerror(errno));
		return false;
	}

	if (::listen(fd, SOMAXCONN) == -1) {
		ERROR_LOG("[Server] listen failed");
		return false;
	}

	return true;
}

bool Server::isServerSocket(int fd) const {
	for (size_t i = 0; i < _server_fds.size(); ++i) {
		if (_server_fds[i] == fd) return true;
	}
	return false;
}

void Server::cleanupClient(int client_fd) {
	std::map<int, Client*>::iterator it = _clients.find(client_fd);
	if (it != _clients.end()) {
		delete it->second;
		_clients.erase(it);
	}
	_event_loop->remove(client_fd);
	::close(client_fd);
}

void Server::cleanupExpiredClients(void) {
	time_t now = ::time(NULL);
	std::vector<int> expired_fds;

	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		if (it->second->isExpired(now)) {
			expired_fds.push_back(it->first);
		}
	}

	for (size_t i = 0; i < expired_fds.size(); ++i) {
		cleanupClient(expired_fds[i]);
	}
}

// Public Functions
bool Server::init(void) {
	if (!_event_loop->init()) {
		ERROR_LOG("[Server] EventLoop init failed");
		return false;
	}
	INFO_LOG("[Server] initialized");
	return true;
}

bool Server::addListenPort(const std::string& host, int port) {
	int fd = createServerSocket();
	if (fd == -1) return false;

	if (!bindAndListen(fd, host, port)) {
		::close(fd);
		return false;
	}

	if (!_event_loop->addServerSocket(fd)) {
		ERROR_LOG("[Server] failed to add socket to EventLoop");
		::close(fd);
		return false;
	}

	_server_fds.push_back(fd);
	_server_ports[fd] = port;

	INFO_LOG("[Server] listening on " << host << ":" << port);
	return true;
}

void Server::run(void) {
	if (_server_fds.empty()) {
		ERROR_LOG("[Server] no listen ports");
		return;
	}

	_running = true;
	_event_loop->run(*this);
}

void Server::stop(void) {
	if (!_running) return;

	_running = false;

	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		delete it->second;
	}
	_clients.clear();

	for (size_t i = 0; i < _server_fds.size(); ++i) {
		::close(_server_fds[i]);
	}
	_server_fds.clear();
	_server_ports.clear();

	INFO_LOG("[Server] stopped");
}

// EventLoop callback functions

void Server::onReadable(int fd) {
    if (isServerSocket(fd)) {
        // 새 연결 처리 함수 호출
        handleNewConnection(fd);
    } else {
        // 기존 클라이언트 데이터 처리 함수 호출
        handleClientData(fd);
    }
}

void Server::handleNewConnection(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = ::accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ERROR_LOG("[Server] accept failed");
        }
        return;
    }

    if (::fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
        ERROR_LOG("[Server] fcntl failed");
        ::close(client_fd);
        return;
    }

    if (!_event_loop->addClientSocket(client_fd)) {
        ERROR_LOG("[Server] failed to add client");
        ::close(client_fd);
        return;
    }

    // server_fd를 키로 사용하여 해당 리슨 포트를 검색
    int listen_port = _server_ports[server_fd];
    Client* client = new Client(client_fd, listen_port);
    _clients[client_fd] = client;
    
    DEBUG_LOG("[Server] client connected: fd=" << client_fd);
}

void Server::handleClientData(int client_fd) {
    std::map<int, Client*>::iterator it = _clients.find(client_fd);
    if (it == _clients.end()) return;

    Client* client = it->second;
    
    // Data Reception
    char buffer[BUFFER_SIZE];
    ssize_t bytes = ::recv(client_fd, buffer, BUFFER_SIZE, 0);

    if (bytes <= 0) {
        // 0: 정상 종료 (FIN), -1: 에러
        if (bytes == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            onHangup(client_fd); // 연결 종료 처리
        }
        return;
    }

    client->appendRawBuffer(buffer, bytes);
    client->updateActivity();

    // Step 1: Parse Headers
    if (client->getHeaderState() == HEADER_INCOMPLETE) {
        if (!client->tryParseHeaders()) return;
        
        DEBUG_LOG("[Server] headers parsed: " 
            << client->getRequest()->getMethod() << " " 
            << client->getRequest()->getUri());
        
        if (client->getState() == WRITING_RESPONSE) {
            _event_loop->setWritable(client_fd, true);
            return;
        }
    }

    // Step 2: Find Config
    if (!client->getServerContext()) {
        HttpRequest* request = client->getRequest();
        
        const ServerContext* serverConf =
            RequestRouter::findServerForRequest(request, client->getPort());
        const LocationContext* locConf = NULL;

        if (serverConf) {
            locConf = RequestRouter::findLocationForRequest(
                serverConf, request->getUri(), request->getMethod()
            );
        }

        client->setServerContext(serverConf);
        client->setLocationContext(locConf);

        if (serverConf && locConf && 
            !RequestRouter::isMethodAllowedInLocation(request->getMethod(), *locConf)) {
            HttpResponse* response = new HttpResponse(
                HttpResponse::createErrorResponse(StatusCode::METHOD_NOT_ALLOWED, serverConf, locConf)
            );
            client->setResponse(response);
            _event_loop->setWritable(client_fd, true);
            return;
        }
    }

    // Step 3: Parse Body
    if (client->getState() != PROCESSING_REQUEST &&
        client->getState() != WRITING_RESPONSE) {
        
        if (!client->tryParseBody()) return;
        
        DEBUG_LOG("[Server] body complete: " << client->getRequest()->getBodyLength() << " bytes");
    }

    // Step 4: Process Request
    if (client->getState() == PROCESSING_REQUEST) {
        HttpRequest* request = client->getRequest();
        const ServerContext* serverConf = client->getServerContext();
        const LocationContext* locConf = client->getLocationContext();

        if (!serverConf) {
            HttpResponse* response = new HttpResponse(
                HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL)
            );
            client->setResponse(response);
        } else {
            HttpResponse* response = HttpController::processRequest(
                request, client->getPort(), serverConf, locConf
            );
            client->setResponse(response);
        }
        
        DEBUG_LOG("[Server] response ready");
    }

    // Set Write Event
    if (client->needsWriteEvent()) {
        _event_loop->setWritable(client_fd, true);
    }
}

void Server::onWritable(int fd) {
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it != _clients.end()) {
		Client* client = it->second;

		if (!client->handleWrite()) {
			onHangup(fd);
		} else if (!client->needsWriteEvent()) {
			_event_loop->setWritable(fd, false);
		}
	}
}

void Server::onHangup(int fd) {
	DEBUG_LOG("[Server] client disconnected: fd=" << fd);
	cleanupClient(fd);
}

void Server::onTick(void) {
	cleanupExpiredClients();
}
