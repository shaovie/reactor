#ifndef OPTIONS_H_
#define OPTIONS_H_

#include <thread>

class options {
public:
    options() = default;

    // reactor option
    int poll_read_buf_size  = 256 * 1024;
    int poll_write_buf_size = 256 * 1024;
    int poller_num = std::thread::hardware_concurrency();

    // timer option
    int timer_init_size = 1024;

    // acceptor option
    bool reuse_addr = true;
    bool reuse_port = false;
    int rcvbuf_size = 0;
    int backlog = 128; // for listen
};

#endif // OPTIONS_H_
