#include "acceptor.h"
#include "poller.h"
#include "options.h"
#include "reactor.h"

#include <cstdio>
#include <string>
#include <errno.h>
#include <string.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>

// addr: "192.168.0.1:8080" or ":8080" or "unix:/tmp/xxxx.sock"
int acceptor::open(const std::string &addr, const options &opt) {
    if (addr.substr(0, 5) == "unix:")
        return this->uds_open(addr.substr(5, addr.length() - 5), opt);
    return this->tcp_open(addr, opt);
}
bool acceptor::on_read() {
    int fd = this->get_fd();
    int conn = -1;
    for (int i = 0; i < 128; ++i) {
        conn = ::accept4(fd, nullptr, nullptr, SOCK_NONBLOCK|SOCK_CLOEXEC);
        if (conn == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == EMFILE) {
                // 句柄不足，暂停200毫秒，不然会一直触发事件
                if (this->get_poller()->schedule_timer(this, 200, 0) == 0) {
                    this->get_poller()->remove(fd, ev_handler::ev_all);
                }
            }
            break;
        }
        auto eh = this->new_conn_func();
        if (eh == nullptr) {
            socket::close(conn);
            break;
        }
        eh->set_fd(conn);
        if (eh->on_open() == false)
            eh->on_close();
    }
    return false;
}
bool acceptor::on_timeout(const int64_t) {
    if (this->get_fd() != -1)
        this->get_poller()->add(this, this->get_fd(), ev_handler::ev_accept);
    return false;
}
// addr: "192.168.0.1:8080" or ":8080"
int acceptor::tcp_open(const std::string &addr, const options &opt) {
    auto p = addr.find(":");
    if (p == addr.npos || p < 7) {
        fprintf(stderr, "reactor: accepor open fail! addr invalid\n");
        return -1;
    }
    std::string ip = addr.substr(0, p);
    int port = 0;
    try {
        port = std::stoi(addr.substr(p+1, addr.length() - p - 1)); // throw
    } catch(...) {
        fprintf(stderr, "reactor: accepor open fail! addr invalid\n");
        return -1;
    }
    if (port < 1 || port > 65535) {
        fprintf(stderr, "reactor: accepor open fail! port invalid\n");
        return -1;
    }

    int fd = ::socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
    if (fd == -1) {
        fprintf(stderr, "reactor: create listen socket fail! %s\n", strerror(errno));
        return -1;
    }
    // `sysctl -a | grep net.ipv4.tcp_rmem` 返回 min default max
    // 默认内核会在min,max之间动态调整, default是初始值, 如果设置了SO_RCVBUF, 
    // 缓冲区大小不变成固定值, 内核也不会进行动态调整了.
    //
    // 必须在listen/connect之前调用
    // must < `sysctl -a | grep net.core.rmem_max`
    if (opt.rcvbuf_size > 0 && socket::set_rcvbuf(fd, opt.rcvbuf_size) != 0) {
        socket::close(fd);
        fprintf(stderr, "reactor: set rcvbuf fail! %s\n", strerror(errno));
        return -1;
    }
    if (opt.reuse_addr && socket::reuseaddr(fd, 1) != 0) {
        socket::close(fd);
        fprintf(stderr, "reactor: set reuseaddr fail! %s\n", strerror(errno));
        return -1;
    }
    if (opt.reuse_port && socket::reuseport(fd, 1) != 0) {
        socket::close(fd);
        fprintf(stderr, "reactor: set reuseport fail! %s\n", strerror(errno));
        return -1;
    }
    struct sockaddr_in laddr;
    ::memset(reinterpret_cast<void *>(&laddr), 0, sizeof(laddr));
    inet_address iaddr(port, ip.c_str());
    laddr = *reinterpret_cast<sockaddr_in *>(iaddr.get_addr());
    return this->listen(fd,
        reinterpret_cast<sockaddr *>(&laddr), sizeof(laddr),
        opt.backlog);
}
int acceptor::uds_open(const std::string &addr, const options &opt) {
    ::remove(addr.c_str());
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        fprintf(stderr, "reactor: create unix socket fail! %s\n", strerror(errno));
        return -1;
    }
    struct sockaddr_un laddr;
    laddr.sun_family = AF_UNIX;
    ::strcpy(laddr.sun_path, addr.c_str());
    return this->listen(fd,
        reinterpret_cast<sockaddr *>(&laddr), sizeof(laddr),
        opt.backlog);
}
int acceptor::listen(const int fd,
    const struct sockaddr *addr, socklen_t addrlen,
    const int backlog) {
    if (::bind(fd, addr, addrlen) == -1) {
        socket::close(fd);
        fprintf(stderr, "reactor: bind fail! %s\n", strerror(errno));
        return -1;
    }

    if (::listen(fd, backlog) == -1) {
        socket::close(fd);
        fprintf(stderr, "reactor: listen fail! %s\n", strerror(errno));
        return -1;
    }

    if (this->get_reactor()->add_ev_handler(this, fd, ev_handler::ev_accept) != 0) {
        socket::close(fd);
        fprintf(stderr, "reactor: add accept ev handler fail! %s\n", strerror(errno));
        return -1;
    }
    return 0;
}
