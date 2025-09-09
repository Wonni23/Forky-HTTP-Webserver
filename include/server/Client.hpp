#ifndef CLIENT_HPP
# define CLIENT_HPP

#include "webserv.hpp"

enum	ClientState {
	READING_REQUEST,
	PROCESSING_REQUEST,
	WRITING_RESPONSE,
	KEEP_ALIVE,
	DISCONNECTED
};

class	Client {
private:
	int			_fd;
	ClientState _state;
	std::string	_request_buffer;
	std::string	_response_buffer;
	size_t		_response_sent;
	time_t		_last_activity;

public:
	Client(int fd);
	~Client();

	bool	handleRead();
	bool	handleWrite();
	bool	isExpired(time_t now) const;

	int		getFd() const { return _fd; }
	void	updateActivity();
};

#endif