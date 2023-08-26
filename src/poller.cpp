#include "poller.h"

#include <sys/epoll.h>

int poller::open(timer_qheap *timer) {
	int fd = ::epoll_create1(EPOLL_CLOEXEC);
	if (fd == -1) {
        fprintf(stderr, "reactor: epoll_create1 fail! %s\n", strerror(errno));
		return -1;
    }

	this->events = new epoll_event[128]();
    this->timer = timer;
    this->efd = fd;
    return 0;
}
void poller::destroy() {
    this->timer->destroy();
    delete[] this->events;
    ::close(this->efd);

    this->timer = nullptr;
    this->events = nullptr;
    this->efd = -1

    delete this;
}
int poller::add(ev_handler *eh, int fd, const uint32_t ev) {
	auto pd = this->poller_desc_map->new_one(fd);
	pd.fd = fd;
	pd.eh = eh;
	this->poller_desc_map->store(fd, pd);

	struct epoll_event epev;
	::memset(&epev, 0, sizeof(epoll_event));
	epev.events = ev;
	epev.data.ptr = pd;

	auto ret = ::epoll_ctl(this->efd, EPOLL_CTL_ADD, fd, &epev);
	if (ret == 0)
		return 0;
	this->poller_desc_map->del(fd);
	return -1
}
void poller::run() { 
    int nfds = 0, msec = -1;
    struct epoll_event *ev_itor = nullptr;
    poller_desc *pd = nullptr;
    ev_handler *eh = nullptr;
    int wait_events_len = sizeof(this->events) / sizeof(this->events[0]);
    while (true) {
         nfds = ::epoll_wait(this->efd, this->events, wait_events_len, msec);
         if (nfds > 0) {
             for (int i = 0; i < nfds; ++i) {
                 ev_itor = this->events + i;
                 pd = (poller_desc*)ev_itor->data.ptr;

                 // EPOLLHUP refer to man 2 epoll_ctl
                 if (ev_itor->events & (EPOLLHUP|EPOLLERR)) {
                     eh = pd->eh;
                     this->remove(pd->fd, ev_all); // MUST before on_close
                     eh.on_close();
                     continue;
                 }

                 // MUST before EPOLLIN (e.g. connect)
                 if (ev_itor->events & (EPOLLOUT)) {
                     if (pd->eh.on_write() == false) {
                         eh = pd->eh;
                         this->remove(pd->fd, ev_all); // MUST before on_close
                         eh.on_close();
                         continue;
                     }
                 }

                 if (ev_itor->events & (EPOLLIN)) {
                     if (pd->eh.on_read() == false) {
                         eh = pd->eh;
                         this->remove(pd->fd, ev_all); // MUST before on_close
                         eh.on_close();
                         continue;
                     }
                 }
             } // end of `for i < nfds'
         } else if (nfds == -1 && errno != EINTR) {
            fprintf(stderr, "reactor: epoll_wait exception! %s\n", strerror(errno));
            break // exit
         }
    }
    this->destroy()
}
