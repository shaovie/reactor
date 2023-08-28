#include "poller.h"
#include "options.h"
#include "ev_handler.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sched.h>
#include <thread>
#include <random>

int poller::open(const options &opt) {
    if (opt.poll_io_buf_size < 1) {
        fprintf(stderr, "reactor: poll_io_buf_size=%d < 1\n", opt.poll_io_buf_size);
        return -1;
    }
    if (opt.timer_init_size < 1) {
        fprintf(stderr, "reactor: timer_init_size=%d < 1\n", opt.timer_init_size);
        return -1;
    }
    if (opt.ready_events_size < 1) {
        fprintf(stderr, "reactor: ready_events_size=%d < 1\n", opt.ready_events_size);
        return -1;
    }
    if (opt.max_fd_estimate < 1) {
        fprintf(stderr, "reactor: max_fd_estimate=%d < 1\n", opt.max_fd_estimate);
        return -1;
    }
    int fd = ::epoll_create1(EPOLL_CLOEXEC);
    if (fd == -1) {
        fprintf(stderr, "reactor: epoll_create1 fail! %s\n", strerror(errno));
        return -1;
    }
    this->efd = fd;

    auto timer = new timer_qheap(opt.timer_init_size);
    if (timer->open() == -1)
        return -1;

    if (this->add(timer, timer->get_fd(), ev_handler::ev_read) != 0) {
        fprintf(stderr, "reactor: add timer to poller fail! %s\n", strerror(errno));
        return -1;
    }
    this->async_sendq = new async_send(256);
    if (this->async_sendq->open(this) != 0)
        return -1;

    this->poll_descs = new poll_desc_map(opt.max_fd_estimate);
    this->timer = timer;
    this->ready_events_size = opt.ready_events_size;
    this->ready_events = new epoll_event[this->ready_events_size]();
    this->io_buf_size = opt.poll_io_buf_size;
    this->io_buf = new char[this->io_buf_size];
    std::default_random_engine dre;
    dre.seed(this->efd);
    this->seq.store(dre());
    return 0;
}
void poller::destroy() {
    if (this->timer != nullptr) {
        delete this->timer;
        this->timer = nullptr;
    }
    if (this->efd != -1) {
        ::close(this->efd);
        this->efd = -1;
    }

    if (this->poll_descs != nullptr) {
        delete this->poll_descs;
        this->poll_descs = nullptr;
    }
    if (this->ready_events != nullptr) {
        delete[] this->ready_events;
        this->ready_events = nullptr;
    }
    if (this->io_buf != nullptr) {
        delete []this->io_buf;
        this->io_buf = nullptr;
    }
}
int poller::add(ev_handler *eh, const int fd, const uint32_t ev) {
    eh->set_poller(this);
    eh->set_seq(this->seq++);
    auto pd = this->poll_descs->new_one(fd);
    pd->fd = fd;
    pd->eh = eh;
    this->poll_descs->store(fd, pd);

    struct epoll_event epev;
    ::memset(&epev, 0, sizeof(epoll_event));
    epev.events = ev;
    epev.data.ptr = pd;

    if (::epoll_ctl(this->efd, EPOLL_CTL_ADD, fd, &epev) == 0)
        return 0;
    this->poll_descs->del(fd);
    return -1;
}
int poller::append(const int fd, const uint32_t ev) {
    auto pd = this->poll_descs->load(fd);
    if (pd == nullptr)
        return -1;

    struct epoll_event epev;
    ::memset(&epev, 0, sizeof(epoll_event));
    epev.events = (pd->events | ev);
    epev.data.ptr = pd;
    if (::epoll_ctl(this->efd, EPOLL_CTL_MOD, fd, &epev) != 0)
        return -1;

    pd->events |= ev;
    return 0;
}
int poller::remove(const int fd, const uint32_t ev) {
    if (ev == ev_handler::ev_all) {
        this->poll_descs->del(fd);
        return ::epoll_ctl(this->efd, EPOLL_CTL_DEL, fd, nullptr);
    }
    auto pd = this->poll_descs->load(fd);
    if (pd == nullptr)
        return -1;
    if ((pd->events & (~ev)) == 0) {
        this->poll_descs->del(fd);
        return ::epoll_ctl(this->efd, EPOLL_CTL_DEL, fd, nullptr);
    }
    struct epoll_event epev;
    ::memset(&epev, 0, sizeof(epoll_event));
    epev.events = (pd->events & (~ev));
    epev.data.ptr = pd;
    if (::epoll_ctl(this->efd, EPOLL_CTL_MOD, fd, &epev) != 0)
        return -1;

    pd->events &= (~ev);
    return 0;
}
void poller::set_cpu_affinity() {
    if (this->cpu_id == -1)
        return;
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(this->cpu_id, &cpu_set);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set) != 0)
        fprintf(stderr, "reactor: set cpu affinity fail! %s\n", strerror(errno));
}
void poller::run() {
    this->thread_id = pthread_self();
    this->set_cpu_affinity();

    int nfds = 0, msec = -1;
    struct epoll_event *ev_itor = nullptr;
    poll_desc *pd = nullptr;
    ev_handler *eh = nullptr;
    while (true) {
        nfds = ::epoll_wait(this->efd, this->ready_events, this->ready_events_size, msec);
        if (nfds > 0) {
            for (int i = 0; i < nfds; ++i) {
                ev_itor = this->ready_events + i;
                pd = (poll_desc*)ev_itor->data.ptr;

                // EPOLLHUP refer to man 2 epoll_ctl
                if (ev_itor->events & (EPOLLHUP|EPOLLERR)) {
                    eh = pd->eh;
                    this->remove(pd->fd, ev_handler::ev_all); // MUST before on_close
                    eh->on_close();
                    continue;
                }

                // MUST before EPOLLIN (e.g. connect)
                if (ev_itor->events & (EPOLLOUT)) {
                    if (pd->eh->on_write() == false) {
                        eh = pd->eh;
                        this->remove(pd->fd, ev_handler::ev_all); // MUST before on_close
                        eh->on_close();
                        continue;
                    }
                }

                if (ev_itor->events & (EPOLLIN)) {
                    if (pd->eh->on_read() == false) {
                        eh = pd->eh;
                        this->remove(pd->fd, ev_handler::ev_all); // MUST before on_close
                        eh->on_close();
                        continue;
                    }
                }
            } // end of `for i < nfds'
        } else if (nfds == -1 && errno != EINTR) {
            fprintf(stderr, "reactor: epoll_wait exception! %s\n", strerror(errno));
            break; // exit
        }
    }
    this->destroy();
}
