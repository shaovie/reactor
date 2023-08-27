#include "connector.h"
#include "poller.h"
#include "reactor.h"

#include "socket.h"
#include "inet_address.h"
#include "ev_handler.h"

class in_progress_connect : public ev_handler {
public:
    in_progress_connect(connector *cn, ev_handler *eh) : cn(cn), eh(eh) { }

    virtual bool on_read() {
        if (this->io_ev_trigger)
            return true;
        this->eh->on_connect_fail(err_connect_fail);
        this->get_poller()->cancel_timer(this);
        this->io_ev_trigger = true;
        return false;
    }
    virtual bool on_write() {
        if (this->io_ev_trigger)
            return true;
        int fd = this->get_fd();
        this->set_fd(-1); // From here on, the `fd` resources will be managed by eh.
        this->get_poller()->remove(fd, ev_handler::ev_all);
        this->get_poller()->cancel_timer(this);
        this->io_ev_trigger = true;

        this->eh->set_fd(fd);
        if (this->eh->on_open() == false)
            this->eh->on_close();
        return true;
    }
    virtual bool on_timeout(const int64_t) {
        if (this->io_ev_trigger)
            return false;
        
        this->timeout_trigger = true;
        this->get_poller()->remove(this->get_fd(), ev_handler::ev_all);
        this->eh->on_connect_fail(err_connect_timeout);
        this->on_close();
        return false;
    }
    virtual void on_close() {
        if (this->timeout_trigger && this->io_ev_trigger) {
            this->destroy();
            delete this;
        }
    }
private:
    bool io_ev_trigger = false;
    bool timeout_trigger = false;
    connector *cn = nullptr;
    ev_handler *eh = nullptr;
};
int connector::connect(ev_handler *eh,
    const inet_address &remote_addr, 
    const int timeout /*milliseconds*/,
    const size_t rcvbuf_size) {

    int fd = ::socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
    if (fd == -1)
        return -1;

    // `sysctl -a | grep net.ipv4.tcp_rmem` 返回 min default max
    // 默认内核会在min,max之间动态调整, default是初始值, 如果设置了SO_RCVBUF, 
    // 缓冲区大小不变成固定值, 内核也不会进行动态调整了.
    //
    // 必须在listen/connect之前调用
    // must < `sysctl -a | grep net.core.rmem_max`
    
    if (rcvbuf_size > 0 && socket::set_rcvbuf(fd, rcvbuf_size) == -1) {
        socket::close(fd);
        return -1;
    }
    int ret = ::connect(fd,
        reinterpret_cast<sockaddr *>(remote_addr.get_addr()),
        remote_addr.get_addr_size());

    if (ret == 0) {
        eh->set_fd(fd); // fd 所有权转移, 从此后eh管理fd
        if (eh->on_open() == false) {
            eh->on_close(); 
            return -1;
        }
        return 0;
    } else if (errno == EINPROGRESS) {
        if (timeout < 1) {
            socket::close(fd);
            return -1;
        }
        return this->nblock_connect(eh, fd, timeout);
    }
    socket::close(fd);
    return -1;
}
int connector::nblock_connect(ev_handler *eh, const int fd, const int timeout) {
    in_progress_connect *ipc = new in_progress_connect(this, eh);

    ipc->set_fd(fd);
    if (this->get_reactor()->add_ev_handler(ipc, fd, ev_handler::ev_connect) == -1) {
        delete ipc;
        socket::close(fd);
        return -1;
    }
    // 要在同一个poller中注册定时器
    //
    // add_ev_handler 和 schedule_timer 不保证原子性,
    // 有可能schedule_timer的时候 ipc已经触发了I/O事件
    ipc->get_poller()->schedule_timer(ipc, timeout, 0);
    return 0;
}
