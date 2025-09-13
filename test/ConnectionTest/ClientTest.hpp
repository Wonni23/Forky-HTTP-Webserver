#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "webserv.hpp"

enum ClientState {
    READING_REQUEST,
    PROCESSING_REQUEST,
    WRITING_RESPONSE,
    KEEP_ALIVE,
    DISCONNECTED
};

class Client {
private:
    int             _fd;
    ClientState     _state;
    std::string     _raw_buffer;
    
    // ✅ HTTP 객체 대신 에코용 데이터
    std::string     _echo_data;
    size_t          _response_sent;
    time_t          _last_activity;
    
    void setState(ClientState new_state);

public:
    Client(int fd);
    ~Client();

    bool handleRead();
    bool handleWrite();
    bool isExpired(time_t now) const;

    void updateActivity();
    bool needsWriteEvent() const;

    int getFd() const { return _fd; }
    ClientState getState() const { return _state; }

    // ✅ 에코용 메서드
    std::string getRawBuffer() const;
    void setEchoData(const std::string& data);
};

#endif
