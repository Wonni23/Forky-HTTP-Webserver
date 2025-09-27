#include "server/Server.hpp"
#include <netinet/in.h>
#include <arpa/inet.h>

Server::Server() : _event_loop(), _running(false) {
	_event_loop = new EventLoop();
}

Server::~Server() {
	stop();
	if (_event_loop) {
		delete _event_loop;
	}
}

/* Private Functions */

int	Server::createServerSocket(void) {
	int	fd;
	int	opt;

	fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
	{
		ERROR_LOG("socket() failed: " << std::strerror(errno));
		return (-1);
	} // Create Server Socket

	opt = 1;
	if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
	{
		ERROR_LOG("setsockopt(SO_REUSEADDR) failed: " << std::strerror(errno));
		::close(fd);
		return (-1);
	} // SO_REUSEADDR setting (it prevents "Address already in use" when restart)

	return (fd);
}

bool	Server::bindAndListen(int fd, const std::string& host, int port)
{
	struct sockaddr_in addr = {};

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	if (host == "0.0.0.0" || host.empty()) {
		addr.sin_addr.s_addr = INADDR_ANY;
	} else {
		if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
			ERROR_LOG("Invalid host address: " << host);
			return false;
		}
	} // Setting Address

	if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		ERROR_LOG("bind() failed for " << host << ":" << port << " - " << std::strerror(errno));
		return false;
	} // Bining

	if (::listen(fd, SOMAXCONN) == -1) {
		ERROR_LOG("listen() failed: " << std::strerror(errno));
		return false;
	} // Listening

	return true;
}

bool	Server::isServerSocket(int fd) const {
	for (size_t i = 0; i < _server_fds.size(); ++i) {
		if (_server_fds[i] == fd) {
			return true;
		}
	}
	return false;
}

void	Server::cleanupClient(int client_fd) {
	DEBUG_LOG("Cleaning up client fd=" << client_fd);

	std::map<int, Client*>::iterator it = _clients.find(client_fd);
	if (it != _clients.end()) {
		delete it->second;
		_clients.erase(it);
	}

	_event_loop->remove(client_fd);
	::close(client_fd);
}

void	Server::cleanupExpiredClients() {
	time_t now = ::time(NULL);
	std::vector<int> expired_fds;

	for (std::map<int, Client*>::iterator it = _clients.begin(); \
		it != _clients.end(); ++it)
	{
		if (it->second->isExpired(now)) {
			expired_fds.push_back(it->first);
		}
	}

	for (size_t i = 0; i < expired_fds.size(); ++i) {
		DEBUG_LOG("Client timeout: fd=" << expired_fds[i]);
		cleanupClient(expired_fds[i]);

	}
}

/* Public Functions */
// Server initializing, Executing
bool	Server::init()
{
	if (!_event_loop->init()) {
		ERROR_LOG("Failed to initialize EventLoop");
		return false;
	}

	INFO_LOG("Server Initialized");
	return true;
}

bool	Server::addListenPort(const std::string& host, int port) {
	int fd = createServerSocket();
	if (fd == -1) {
		return false;
	}

	if (!bindAndListen(fd, host, port)) {
		::close(fd);
		return false;
	}

	if (!_event_loop->addServerSocket(fd)) {
		ERROR_LOG("Failed to add server socket to Eventloop");
		::close(fd);
		return false;
	} // Adding listening socket to epoll

	_server_fds.push_back(fd);
	_server_ports[fd] = port;

	INFO_LOG("Server listening on " << host << ":" << port);
	
	return true;
}

void	Server::run() {
	if (_server_fds.empty()) {
		ERROR_LOG("No listen ports configured");
		return ;
	}

	_running = true;
	_event_loop->run(*this);
}

void	Server::stop() {
	if (!_running) return ;

	_running = false;

	// Cleanup Clients
	for (std::map<int, Client*>::iterator it = _clients.begin(); \
		it != _clients.end(); ++it) {
			delete it->second;
	}
	_clients.clear();

	// Closing server sockets
	for (size_t i = 0; i < _server_fds.size(); ++i) {
		::close(_server_fds[i]);
	}
	_server_fds.clear();
	_server_ports.clear();

	INFO_LOG("Server Stopped");
}


// ============ EventLoop callback functions ===============
void	Server::onReadable(int fd) {
	if (isServerSocket(fd)) {
		// Accept new client connection
		struct sockaddr_in	client_addr;
		socklen_t			client_len = sizeof(client_addr);

		int client_fd = ::accept(fd, (struct sockaddr*)&client_addr, &client_len);
		if (client_fd == -1) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				ERROR_LOG("accept() failed: " << std::strerror(errno));
			}
			return ;
		}

		if (!_event_loop->addClientSocket(client_fd)) {
			ERROR_LOG("Failed to add client socket to EventLoop");
			::close(client_fd);
			return ;
		}

		// Create client object
		int listen_port = _server_ports[fd];
		Client* client = new Client(client_fd, listen_port);
		_clients[client_fd] = client;

		DEBUG_LOG("New client connected: fd=" << client_fd);
	}
	else
	{
		std::map<int, Client*>::iterator it = _clients.find(fd);
		if (it != _clients.end()) {
			Client* client = it->second;

			if (!client->handleRead()) {
				onHangup(fd);
				return ;
			}

			if (client->getState() == PROCESSING_REQUEST) {
				HttpRequest* request = client->getRequest();

				HttpResponse* response = HttpController::handleRequest(request);
				client->setResponse(response);
			}

			if (client->needsWriteEvent()) {
				_event_loop->setWritable(fd, true);
			}
		}
	}
}

void	Server::onWritable(int fd) {
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

void	Server::onHangup(int fd)
{
	DEBUG_LOG("Client fd=" << fd << " disconnected");
	cleanupClient(fd);
}

void	Server::onTick() {
	cleanupExpiredClients();
}
