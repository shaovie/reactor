#ifndef ACCEPTOR_H_
#define ACCEPTOR_H_

#include "ev_handler.h"

#include <string>

// Forward declarations
class reactor;
class options;
struct sockaddr;

class acceptor : public ev_handler {
public:
    typedef ev_handler* (*new_conn_func_t)();

    acceptor() = delete;
    acceptor(reactor *r, new_conn_func_t f) : new_conn_func(f) { this->set_reactor(r); }

    // addr: "192.168.0.1:8080" or ":8080" or "unix:/tmp/xxxx.sock"
    int open(const std::string &addr, const options &opt);

    virtual bool on_read();

    virtual bool on_timeout(const int64_t);
protected:
    // addr: "192.168.0.1:8080" or ":8080"
    int tcp_open(const std::string &addr, const options &opt);

    int uds_open(const std::string &addr, const options &opt);

    int listen(const int fd,
        const struct sockaddr *addr, socklen_t addrlen,
        const int backlog);
private:
    new_conn_func_t new_conn_func;
};

#endif // ACCEPTOR_H_
