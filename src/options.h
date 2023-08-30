#ifndef OPTIONS_H_
#define OPTIONS_H_

#include <thread>

class options {
public:
    options() = default;

    // reactor option
    bool set_cpu_affinity = true;
    bool with_timer_shared = false;
    int poll_io_buf_size  = 256 * 1024; // for read & write sync i/o
    int poller_num = std::thread::hardware_concurrency();
    int ready_events_size = 128; // epoll_wait 返回多少准备好的事件
    int max_fd_estimate = 1024*20; // fds 预估最大数量，超过此数量会用map保存数据（有锁)

    // timer option
    int timer_init_size = 1024;

    // acceptor option
    bool reuse_addr = true;
    bool reuse_port = false;
    int rcvbuf_size = 0;
    int backlog = 128; // for listen
};

#endif // OPTIONS_H_
