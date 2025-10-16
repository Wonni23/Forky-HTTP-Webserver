#include "server/Server.hpp"
#include "http/HttpController.hpp"
#include <netinet/in.h>
#include <arpa/inet.h>


Server::Server() : _event_loop(), _running(false) {
	_event_loop = new EventLoop();
	DEEP_LOG("[Server] created");
}


Server::~Server() {
	stop();
	if (_event_loop) {
		delete _event_loop;
	}
	DEEP_LOG("[Server] destroyed");
}


/* Private Functions */


int Server::createServerSocket(void) {
	int fd;
	int opt;


	fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
	{
		ERROR_LOG("[Server] socket() failed: " << std::strerror(errno));
		return (-1);
	}
	
	DEEP_LOG("[Server] server socket created fd=" << fd);


	opt = 1;
	if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		ERROR_LOG("[Server] setsockopt(SO_REUSEADDR) failed for fd=" << fd << ": " << std::strerror(errno));
		::close(fd);
		return (-1);
	}

	DEEP_LOG("[Server] fd=" << fd << " SO_REUSEADDR enabled");
	return (fd);
}


bool Server::bindAndListen(int fd, const std::string& host, int port)
{
	struct sockaddr_in addr = {};


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
		ERROR_LOG("[Server] bind() failed for " << host << ":" << port << " fd=" << fd << " - " << std::strerror(errno));
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


void Server::cleanupExpiredClients() {
	time_t now = ::time(NULL);
	std::vector<int> expired_fds;


	for (std::map<int, Client*>::iterator it = _clients.begin(); \
		it != _clients.end(); ++it)
	{
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


/* Public Functions */
bool Server::init()
{
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


void Server::run() {
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


void Server::stop() {
	if (!_running) return ;


	DEBUG_LOG("[Server] stopping...");
	_running = false;


	// Cleanup Clients
	DEBUG_LOG("[Server] cleaning up " << _clients.size() << " clients");
	for (std::map<int, Client*>::iterator it = _clients.begin(); \
		it != _clients.end(); ++it) {
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



// ============ EventLoop callback functions ===============
void Server::onReadable(int fd) {
	if (isServerSocket(fd)) {
		DEEP_LOG("[Server] server socket fd=" << fd << " readable, accepting new connection");
		
		struct sockaddr_in  client_addr;
		socklen_t           client_len = sizeof(client_addr);


		int client_fd = ::accept(fd, (struct sockaddr*)&client_addr, &client_len);
		if (client_fd == -1) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				ERROR_LOG("[Server] accept() failed on fd=" << fd << ": " << std::strerror(errno));
			}
			return ;
		}

		char client_ip[INET_ADDRSTRLEN];
		::inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
		DEEP_LOG("[Server] accepted connection from " << client_ip << ":" << ntohs(client_addr.sin_port) << " on fd=" << client_fd);


		if (!_event_loop->addClientSocket(client_fd)) {
			ERROR_LOG("[Server] failed to add client socket fd=" << client_fd << " to EventLoop");
			::close(client_fd);
			return ;
		}


		// Create client object
		int listen_port = _server_ports[fd];
		Client* client = new Client(client_fd, listen_port);
		_clients[client_fd] = client;


		DEBUG_LOG("[Server] new client: fd=" << client_fd << " port=" << listen_port << " total=" << _clients.size());
	}
	else
	{
		std::map<int, Client*>::iterator it = _clients.find(fd);
		if (it != _clients.end()) {
			Client* client = it->second;

			DEEP_LOG("[Server] client fd=" << fd << " readable");

			if (!client->handleRead()) {
				DEEP_LOG("[Server] client fd=" << fd << " handleRead returned false");
				onHangup(fd);
				return ;
			}


			if (client->getState() == PROCESSING_REQUEST) {
				DEBUG_LOG("[Server] processing request for fd=" << fd);
				HttpRequest* request = client->getRequest();


				HttpResponse* response = HttpController::processRequest(request, client->getPort());
				client->setResponse(response);
				DEEP_LOG("[Server] response set for fd=" << fd);
			}


			if (client->needsWriteEvent()) {
				DEEP_LOG("[Server] fd=" << fd << " needs write event");
				_event_loop->setWritable(fd, true);
			}
		} else {
			DEBUG_LOG("[Server] fd=" << fd << " not found in clients map");
		}
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


void Server::onHangup(int fd)
{
	DEBUG_LOG("[Server] client fd=" << fd << " disconnected");
	cleanupClient(fd);
}


void    Server::onTick() {
	cleanupExpiredClients();
}
