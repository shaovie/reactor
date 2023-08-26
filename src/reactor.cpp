#include "reactor.h"
#include "poller.h"
#include "options.h"
#include "timer_qheap.h"
#include "ev_handler.h"

#include <errno.h>
#include <string.h>
#include <cstdio>
#include <thread>

reactor::reactor(int poller_n) {
    this->poller_num = poller_n;
    this->pollers = new poller[poller_n]();
}
int reactor::open(const options &opt) {
    for (int i = 0; i < this->poller_num; ++i) {
        if (this->pollers[i].open(opt.timer_init_size) != 0) {
            // destroy ?
            return -1;
        }
    }
    return 0;
}
int reactor::add_ev_handler(ev_handler *eh, const int fd, const uint32_t events) {
    if (fd < 0 || eh == nullptr || events == 0)
        return -1;
    int i = 0;
    if (this->poller_num > 0)
        i = fd % this->poller_num;
    eh->set_reactor(this);
    return this->pollers[i].add(eh, fd, events);
}
int reactor::append_ev(const int fd, const uint32_t events) {
    if (fd < 0 || events == 0)
        return -1;
    int i = 0;
    if (this->poller_num > 0)
        i = fd % this->poller_num;
    return this->pollers[i].append(fd, events);
}
int reactor::remove_ev(const int fd, const uint32_t events) {
    if (fd < 0 || events == 0)
        return -1;
    int i = 0;
    if (this->poller_num > 0)
        i = fd % this->poller_num;
    return this->pollers[i].remove(fd, events);
}
void reactor::run() {
    std::thread **threads = new std::thread*[this->poller_num]();
    for (int i = 0; i < this->poller_num; ++i) {
        threads[i] = new std::thread(&poller::run, &(this->pollers[i]));
    }
    for (int i = 0; i < this->poller_num; ++i) {
        threads[i]->join();
        delete threads[i];
    }
    delete[] threads;
}
