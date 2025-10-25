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

// ========= 생성자 및 소멸자 =======
Server::Server(void)
	: _event_loop(NULL), _sessionManager(NULL), _running(false) {
	_event_loop = new EventLoop();
	_sessionManager = new SessionManager();
}

Server::~Server(void) {
	stop();
	if (_sessionManager) {
		delete _sessionManager;
	}
	if (_event_loop) {
		delete _event_loop;
	}
	DEEP_LOG("[Server] destroyed");
}

// ========= Private Functions =======
int Server::createServerSocket(void) {
	int fd;
	int opt;

	fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		ERROR_LOG("[Server] socket() failed: " << std::strerror(errno));
		return (-1);
	}

	DEEP_LOG("[Server] server socket created fd=" << fd);
	opt = 1;
	if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		ERROR_LOG("[Server] setsockopt(SO_REUSEADDR) failed for fd=" << fd << ": " << std::strerror(errno));
		::close(fd);
		return (-1);
	}

	if (::fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		ERROR_LOG("[Server] fcntl(O_NONBLOCK) failed for fd=" << fd);
		::close(fd);
		return (-1);
	}

	return (fd);
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
			ERROR_LOG("[Server] invalid host address: " << host);
			return false;
		}
	}

	if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		ERROR_LOG("[Server] bind() failed for " << host << ":" << port << " fd=" << fd
				 << " - " << std::strerror(errno));
		return false;
	}

	DEEP_LOG("[Server] fd=" << fd << " bound to " << host << ":" << port);

	if (::listen(fd, SOMAXCONN) == -1) {
		ERROR_LOG("[Server] listen() failed for fd=" << fd << ": " << std::strerror(errno));
		return false;
	}

	DEEP_LOG("[Server] fd=" << fd << " listening with backlog=" << SOMAXCONN);
	return true;
}

bool Server::isServerSocket(int fd) const {
	for (size_t i = 0; i < _server_fds.size(); ++i) {
		if (_server_fds[i] == fd) {
			return true;
		}
	}
	return false;
}

void Server::cleanupClient(int client_fd) {
	DEBUG_LOG("[Server] cleaning up client fd=" << client_fd);

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

	if (!expired_fds.empty()) {
		DEBUG_LOG("[Server] found " << expired_fds.size() << " expired clients");
	}

	for (size_t i = 0; i < expired_fds.size(); ++i) {
		DEBUG_LOG("[Server] client timeout: fd=" << expired_fds[i]);
		cleanupClient(expired_fds[i]);
	}
}

// ========= Public Functions =======
bool Server::init(void) {
	if (!_event_loop->init()) {
		ERROR_LOG("[Server] failed to initialize EventLoop");
		return false;
	}

	INFO_LOG("[Server] initialized");
	return true;
}

bool Server::addListenPort(const std::string& host, int port) {
	DEEP_LOG("[Server] adding listen port " << host << ":" << port);

	int fd = createServerSocket();
	if (fd == -1) {
		return false;
	}

	if (!bindAndListen(fd, host, port)) {
		::close(fd);
		return false;
	}

	if (!_event_loop->addServerSocket(fd)) {
		ERROR_LOG("[Server] failed to add server socket fd=" << fd << " to Eventloop");
		::close(fd);
		return false;
	}

	_server_fds.push_back(fd);
	_server_ports[fd] = port;

	INFO_LOG("[Server] listening on " << host << ":" << port << " fd=" << fd);

	return true;
}

void Server::run(void) {
	if (_server_fds.empty()) {
		ERROR_LOG("[Server] no listen ports configured");
		return ;
	}

	if (!_event_loop) {
		ERROR_LOG("[Server] EventLoop is NULL");
		return ;
	}

	DEBUG_LOG("[Server] starting with " << _server_fds.size() << " listen ports");
	_running = true;
	_event_loop->run(*this);
}

void Server::stop(void) {
	if (!_running) {
		return ;
	}

	DEBUG_LOG("[Server] stopping...");
	_running = false;

	// Cleanup Clients
	DEBUG_LOG("[Server] cleaning up " << _clients.size() << " clients");
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
		delete it->second;
	}
	_clients.clear();

	// Closing server sockets
	DEBUG_LOG("[Server] closing " << _server_fds.size() << " server sockets");
	for (size_t i = 0; i < _server_fds.size(); ++i) {
		::close(_server_fds[i]);
	}
	_server_fds.clear();
	_server_ports.clear();

	INFO_LOG("[Server] stopped");
}

// ========= EventLoop callback functions =======
void Server::onReadable(int fd) {
	// ========= Server Socket (Accept) =======
	if (isServerSocket(fd)) {
		struct sockaddr_in  client_addr;
		socklen_t           client_len = sizeof(client_addr);

		int client_fd = ::accept(fd, (struct sockaddr*)&client_addr, &client_len);
		if (client_fd == -1) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				ERROR_LOG("[Server] accept() failed: " << std::strerror(errno));
			}
			return ;
		}

		// Non-blocking 설정
		if (::fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
			ERROR_LOG("[Server] fcntl() failed for client fd=" << client_fd);
			::close(client_fd);
			return ;
		}

		char client_ip[INET_ADDRSTRLEN];
		::inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
		DEEP_LOG("[Server] accepted connection from " << client_ip << ":"
				<< ntohs(client_addr.sin_port) << " on fd=" << client_fd);

		if (!_event_loop->addClientSocket(client_fd)) {
			ERROR_LOG("[Server] failed to add client socket to EventLoop");
			::close(client_fd);
			return ;
		}

		int listen_port = _server_ports[fd];
		Client* client = new Client(client_fd, listen_port);
		_clients[client_fd] = client;

		DEBUG_LOG("[Server] new client: fd=" << client_fd << " port=" << listen_port
				<< " total=" << _clients.size());
		return ;
	}

	// ========= Client Socket (Data) =======
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it == _clients.end()) {
		DEBUG_LOG("[Server] fd=" << fd << " not found in clients map");
		return ;
	}

	Client* client = it->second;
	
	// ========= Data Reception =======
	char buffer[BUFFER_SIZE];
	ssize_t bytes = ::recv(fd, buffer, BUFFER_SIZE - 1, 0);

	if (bytes <= 0) {
		if (bytes == 0) {
			DEBUG_LOG("[Server] client closed: fd=" << fd);
		} else if (errno != EAGAIN && errno != EWOULDBLOCK) {
			ERROR_LOG("[Server] recv error: " << std::strerror(errno));
		}
		onHangup(fd);
		return ;
	}

	buffer[bytes] = '\0';
	client->appendRawBuffer(buffer, bytes);
	client->updateActivity();

	DEBUG_LOG("[Server] received " << bytes << " bytes from fd=" << fd);

	// ========= Step 1: Parse Headers =======
	if (client->getHeaderState() == HEADER_INCOMPLETE) {
		DEBUG_LOG("[Server] fd=" << fd << " parsing headers");

		if (!client->tryParseHeaders()) {
			DEBUG_LOG("[Server] fd=" << fd << " headers incomplete, waiting for more data");
			return ;
		}

		// ✅ 추가: 에러 응답이 설정되었는지 체크
		if (client->getState() == WRITING_RESPONSE && client->needsWriteEvent()) {
			DEBUG_LOG("[Server] fd=" << fd << " error response set during header parsing");
			_event_loop->setWritable(fd, true);
			return ;
		}

		DEBUG_LOG("[Server] fd=" << fd << " headers complete: "
				<< client->getRequest()->getMethod() << " "
				<< client->getRequest()->getUri());
	}

	// ========= Step 2: Find Server/Location Config =======
	if ((client->getHeaderState() == HEADER_COMPLETE || 
	     client->getHeaderState() == BODY_RECEIVING) &&
		!client->getServerContext()) {

		DEBUG_LOG("[Server] fd=" << fd << " finding server/location config");

		HttpRequest* request = client->getRequest();
		if (!request) {
			ERROR_LOG("[Server] Request is NULL");
			onHangup(fd);
			return ;
		}

		const ServerContext* serverConf =
			RequestRouter::findServerForRequest(request, client->getPort());
		const LocationContext* locConf = NULL;

		if (serverConf) {
			locConf = RequestRouter::findLocationForRequest(
				serverConf,
				request->getUri(),
				request->getMethod()
			);
			DEBUG_LOG("[Server] fd=" << fd << " location: "
					<< (locConf ? locConf->path : "NOT_FOUND"));
		} else {
			ERROR_LOG("[Server] no server config for port=" << client->getPort());
		}

		client->setServerContext(serverConf);
		client->setLocationContext(locConf);

		DEBUG_LOG("[Server] fd=" << fd << " config set");

		// 메서드 허용 여부 즉시 확인
		if (serverConf && locConf && 
			!RequestRouter::isMethodAllowedInLocation(request->getMethod(), *locConf)) {

			ERROR_LOG("[Server] method not allowed: " << request->getMethod() << " in location " << locConf->path);

			HttpResponse* response = new HttpResponse(
				HttpResponse::createErrorResponse(StatusCode::METHOD_NOT_ALLOWED, serverConf, locConf)
			);
			client->setResponse(response);

			if (client->needsWriteEvent()) {
				_event_loop->setWritable(fd, true);
			}
			return ;
		}
	}

	// ========= Step 3: Parse Body =======
	// 핵심: REQUEST_COMPLETE나 PROCESSING_REQUEST가 아니면 계속 시도!
	if (client->getHeaderState() != REQUEST_COMPLETE &&
		client->getState() != PROCESSING_REQUEST &&
		client->getState() != WRITING_RESPONSE) {

		DEBUG_LOG("[Server] fd=" << fd << " parsing body");

		if (!client->tryParseBody()) {
			DEBUG_LOG("[Server] fd=" << fd << " body incomplete, waiting for more data");
			return ;
		}

		DEBUG_LOG("[Server] fd=" << fd << " body complete, body size="
				<< client->getRequest()->getBody().length());
	}

	// ========= Step 4: Process Request =======
	if (client->getState() == PROCESSING_REQUEST) {
		DEBUG_LOG("[Server] fd=" << fd << " processing request");

		HttpRequest* request = client->getRequest();
		const ServerContext* serverConf = client->getServerContext();
		const LocationContext* locConf = client->getLocationContext();

		if (!serverConf) {
			ERROR_LOG("[Server] serverConf is NULL for fd=" << fd);
			HttpResponse* errorResponse = new HttpResponse(
				HttpResponse::createErrorResponse(StatusCode::INTERNAL_SERVER_ERROR, NULL, NULL)
			);
			client->setResponse(errorResponse);
		} else {
			HttpResponse* response = HttpController::processRequest(
				request,
				client->getPort(),
				serverConf,
				locConf
			);
			client->setResponse(response);
		}

		DEEP_LOG("[Server] response set for fd=" << fd);
	}

	// ========= Set Write Event =======
	if (client->needsWriteEvent()) {
		DEEP_LOG("[Server] fd=" << fd << " needs write event, setting writable");
		_event_loop->setWritable(fd, true);
	}
}



void Server::onWritable(int fd) {
	DEEP_LOG("[Server] fd=" << fd << " writable");

	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it != _clients.end()) {
		Client* client = it->second;

		if (!client->handleWrite()) {
			DEEP_LOG("[Server] fd=" << fd << " handleWrite returned false");
			onHangup(fd);
		} else if (!client->needsWriteEvent()) {
			DEEP_LOG("[Server] fd=" << fd << " no longer needs write event");
			_event_loop->setWritable(fd, false);
		}
	} else {
		DEBUG_LOG("[Server] fd=" << fd << " not found in clients map");
	}
}

void Server::onHangup(int fd) {
	DEBUG_LOG("[Server] client fd=" << fd << " disconnected");
	cleanupClient(fd);
}

void Server::onTick(void) {
	cleanupExpiredClients();
}
