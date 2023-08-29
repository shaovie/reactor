#include "poll_sync_opt.h"
#include "poller.h"

#include <sys/eventfd.h>

poll_sync_opt::~poll_sync_opt() {
   if (this->efd != -1)
        ::close(this->efd);
    
   if (this->readq != nullptr)
       delete this->readq;
   if (this->writeq != nullptr)
       delete this->writeq;
}
void poll_sync_opt::init(poll_sync_opt::opt_arg &&arg) {
    this->do_sync(arg);
}
int poll_sync_opt::open(poller *poll) {
    int fd = ::eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
    if (fd == -1) {
        fprintf(stderr, "reactor: create eventfd fail! %s\n", strerror(errno));
        return -1;
    }
    if (poll->add(this, fd, ev_handler::ev_read) == -1) {
        fprintf(stderr, "reactor: add eventfd to poller fail! %s\n", strerror(errno));
        ::close(fd);
        return -1;
    }
    this->efd = fd;
    return 0;
}
void poll_sync_opt::push(poll_sync_opt::opt_arg &&arg) {
    this->mtx.lock();
    this->writeq->push_back(std::move(arg));
    this->mtx.unlock();

    int expected = 0;
    if (!this->notified.compare_exchange_strong(expected, 1))
        return ;
    int64_t v = 1;
    int ret = 0;
    do {
        ret = ::write(this->efd, (void *)&v, sizeof(v));
    } while (ret == -1 && errno == EINTR);
}
void poll_sync_opt::do_sync(const poll_sync_opt::opt_arg &arg) {
    if (arg.type == poll_sync_opt::sync_cache_t) {
        auto p = (poll_sync_opt::sync_cache *)arg.arg;
        this->poll->poll_cache_set(p->id, p->value, p->free_func);
        delete p;
    }
}
bool poll_sync_opt::on_read() {
    if (this->readq->empty()) {
        this->mtx.lock();
        std::swap(this->readq, this->writeq);
        this->mtx.unlock();
    }
    for (auto i = 0; i < 8 && !this->readq->empty(); ++i) {
        poll_sync_opt::opt_arg &arg = this->readq->front();
        this->do_sync(arg);
        this->readq->pop_front();
    }
    if (!this->readq->empty()) // Ignore readable eventfd, continue
        return true;

    int64_t v = 0;
    int ret = 0;
    do {
        ret = ::read(this->efd, (void *)&v, sizeof(v));
    } while (ret == -1 && errno == EINTR);

    this->notified.store(0);
    return true;
}
