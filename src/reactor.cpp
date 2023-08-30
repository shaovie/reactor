#include "reactor.h"
#include "poller.h"
#include "options.h"
#include "timer_shared.h"
#include "ev_handler.h"

#include <errno.h>
#include <string.h>
#include <cstdio>
#include <thread>

int reactor::open(const options &opt) {
    if (opt.poller_num < 1) {
        fprintf(stderr, "reactor: poller_num=%d < 1\n", opt.poller_num);
        return -1;
    }
    this->poller_num = opt.poller_num;
    this->pollers = new poller[this->poller_num]();

    int cpu_num = std::thread::hardware_concurrency();
    for (int i = 0; i < this->poller_num; ++i) {
        if (opt.set_cpu_affinity) {
            int cpu_id = i % cpu_num;
            this->pollers[i].set_cpu_id(cpu_id);
        }
        if (this->pollers[i].open(opt) != 0) {
            return -1; // destroy ?
        }
    }
    if (opt.with_timer_shared == true) {
        this->timer = new timer_shared(opt.timer_init_size);
        if (this->timer->open() != 0)
            return -1; // destroy ?
        if (this->add_ev_handler(this->timer, this->timer->get_fd(), ev_handler::ev_read) != 0)
            return -1; // destroy ?
    }
    return 0;
}
int reactor::schedule_timer(ev_handler *eh, const int delay, const int interval) {
    return this->timer->schedule(eh, delay, interval);
}
void reactor::cancel_timer(ev_handler *eh) {
    this->timer->cancel(eh);
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
void reactor::run(const bool join) {
    if (!join) {
        for (int i = 0; i < this->poller_num; ++i) {
            std::thread thr(&poller::run, &(this->pollers[i]));
            thr.detach();
        }
        return ;
    }
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
void reactor::init_poll_sync_opt(const int t, void* args[]) {
    for (int i = 0; i < this->poller_num; ++i)
        this->pollers[i].init_poll_sync_opt(t, args[i]);
}
void reactor::poll_sync_opt(const int t, void* args[]) {
    for (int i = 0; i < this->poller_num; ++i)
        this->pollers[i].do_poll_sync_opt(t, args[i]);
}
