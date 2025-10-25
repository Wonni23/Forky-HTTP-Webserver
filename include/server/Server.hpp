#ifndef SERVER_HPP
# define SERVER_HPP

#include "webserv.hpp"
#include "EventLoop.hpp"
#include "Client.hpp"
#include "SessionManager.hpp"

class	Server {
private:
	EventLoop*				_event_loop;
	SessionManager*			_sessionManager;
	std::vector<int>		_server_fds;	// Server sockets
	std::map<int, Client*>	_clients;		// fd -> Client mapping
	std::map<int, int>		_server_ports;	// fd -> port mapping
	bool					_running;

	// Setting server sockets
	int		createServerSocket(void);
	bool	bindAndListen(int fd, const std::string& host, int port);
	bool	isServerSocket(int fd) const;

	// Cleanup clients
	void	cleanupClient(int client_fd);
	void	cleanupExpiredClients();

public:
	Server();
	~Server();

	// Server initializing, Executing
	bool	init();
	bool	addListenPort(const std::string& host, int port);
	void	run();
	void	stop();

	// EventLoop callback functions
	void	onReadable(int fd);
	void	onWritable(int fd);
	void	onHangup(int fd);
	void	onTick();

	// SessionManager 접근
	SessionManager*	getSessionManager() const { return _sessionManager; }
};

#endif