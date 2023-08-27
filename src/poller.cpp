#include "poller.h"
#include "ev_handler.h"
#include "timer_qheap.h"
#include "poll_desc.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>

#define ready_events_size 128

int poller::open(const int timer_init_size) {
    int fd = ::epoll_create1(EPOLL_CLOEXEC);
    if (fd == -1) {
        fprintf(stderr, "reactor: epoll_create1 fail! %s\n", strerror(errno));
        return -1;
    }
    this->efd = fd;

    auto timer = new timer_qheap(timer_init_size);
    if (timer->open() == -1)
        return -1;

    if (this->add(timer, timer->get_fd(), ev_handler::ev_read) != 0) {
        fprintf(stderr, "reactor: add timer to poller fail! %s\n", strerror(errno));
        return -1;
    }

    this->timer = timer;
    this->ready_events = new epoll_event[ready_events_size]();
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

    if (this->ready_events != nullptr) {
        delete[] this->ready_events;
        this->ready_events = nullptr;
    }
}
int poller::schedule_timer(ev_handler *eh, const int delay, const int interval) {
    return this->timer->schedule(eh, delay, interval);
}
void poller::cancel_timer(ev_handler *eh) {
    this->timer->cancel(eh);
}
int poller::add(ev_handler *eh, const int fd, const uint32_t ev) {
    eh->set_poller(this);
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
    if (pd == nullptr) {
        return -1;
    }

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
void poller::run() {
    int nfds = 0, msec = -1;
    struct epoll_event *ev_itor = nullptr;
    poll_desc *pd = nullptr;
    ev_handler *eh = nullptr;
    while (true) {
        nfds = ::epoll_wait(this->efd, this->ready_events, ready_events_size, msec);
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
