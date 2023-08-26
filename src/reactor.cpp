#include "reactor.h"

#include <errno.h>

#include <cstdio>
#include <thread>

reactor::reactor(int poller_n) {
    this->poller_num = poller_n;
    this->pollers = new poller[poller_n]();
}
int reactor::open(options opt) {
    for (int i = 0; i < this->poller_num; ++i) {
        auto timer = new timer_qheap(opt.timer_init_size);
        if (this->pollers[i].open(timer) != 0) {
            return -1;
        }
        if (this->pollers[i].add(timer, timer.timerfd(), ev_read) != 0) {
            fprintf(stderr, "reactor: add timerfd fail! %s\n", strerror(errno));
            return -1;
        }
    }
    return 0
}
int reactor::add_ev_handler(ev_handler *eh, int fd, uint32_t events) {
}
int reactor::append_ev(int fd, uint32_t events) {
}
int reactor::remove_ev(int fd, uint32_t events) {
}
void reactor::run() {
    std::thread **threads = new std::threads*[this->poller_num]();
    for (int i = 0; i < this->poller_num; ++i) {
        threads[i] = new std::thread(&poller::run, &(this->pollers[i]));
    }
    for (int i = 0; i < this->poller_num; ++i) {
        threads[i]->join();
        delete threads[i];
    }
    delete[] threads;
}
