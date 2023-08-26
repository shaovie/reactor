#ifndef I_SOCKET_H_
#define I_SOCKET_H_

#include "inet_address.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

class socket
{
public:
    static int recv(const int fd, void *buff, const size_t len) {
        int ret = 0;
        do {
            ret = ::recv(fd, buff, len, 0);
        } while (ret == -1 && errno == EINTR);
        return ret;
    }
    static int send(const int fd, const void *buff, const size_t len) {
        int ret = 0;
        do {
            ret = ::send(fd, buff, len, 0);
        } while (ret == -1 && errno == EINTR);
        return ret;
    }
    static inline int close(const int fd) { return ::close(fd); }

    static inline int get_local_addr(const int fd, inet_address &local_addr) {
        int l = local_addr.get_addr_size();
        struct sockaddr *addr = 
            reinterpret_cast<struct sockaddr*>(local_addr.get_addr());
        if (::getsockname(fd, addr, (socklen_t *)&l) == -1)
            return -1;
        return 0;
    }
    static inline int get_remote_addr(const int fd, inet_address &remote_addr) {
        int l = remote_addr.get_addr_size();
        struct sockaddr *addr = 
            reinterpret_cast<struct sockaddr*> (remote_addr.get_addr());
        if (::getpeername(fd, addr, (socklen_t *)&l) == -1)
            return -1;
        return 0;
    }
    static inline int reuseaddr(const int fd, const int val) {
        return ::setsockopt(fd,
            SOL_SOCKET,
            SO_REUSEADDR, 
            (const void*)&val,
            sizeof(val));
    }
    static inline int reuseport(const int fd, const int val) {
        return ::setsockopt(fd,
            SOL_SOCKET,
            SO_REUSEPORT, 
            (const void*)&val,
            sizeof(val));
    }
    static inline int set_nonblock(const int fd) {
        int flag = ::fcntl(fd, F_GETFL, 0);
        if (flag == -1) return -1;
        if (flag & O_NONBLOCK) // already nonblock
            return 0;
        return ::fcntl(fd, F_SETFL, flag | O_NONBLOCK);
    }
    static inline int set_block(const int fd) {
        int flag = ::fcntl(fd, F_GETFL, 0);
        if (flag == -1) return -1;
        if (flag & O_NONBLOCK) // already nonblock
            return ::fcntl(fd, F_SETFL, flag & (~O_NONBLOCK));
        return 0;
    }
    static inline int set_rcvbuf(const int fd, const size_t size) {
        if (size == 0) return -1;
        return ::setsockopt(fd,
            SOL_SOCKET, 
            SO_RCVBUF, 
            (const void*)&size, 
            sizeof(size));
    }
    static inline int set_sndbuf(const int fd, const size_t size) {
        if (size == 0) return -1;
        return ::setsockopt(fd,
            SOL_SOCKET,
            SO_SNDBUF, 
            (const void*)&size, 
            sizeof(size));
    }
    static inline int set_nodelay(const int fd) {
        int flag = 1;
        return ::setsockopt(fd,
            IPPROTO_TCP,
            TCP_NODELAY, 
            (void *)&flag, 
            sizeof(flag));
    }
    static inline int getsock_error(const int fd, int &err) {
        socklen_t len = sizeof(int);
        return ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
    }
};

#endif // I_SOCKET_H_
