#include "async_send.h"
#include "poll_desc.h"
#include "poller.h"

#include <sys/eventfd.h>

async_send::~async_send() {
   if (this->efd != -1)
        ::close(this->efd);
    
   if (this->readq != nullptr)
       delete this->readq;
   if (this->writeq != nullptr)
       delete this->writeq;
}
int async_send::open(poller *poll) {
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
void async_send::push(async_send::item &&asi) {
    this->mtx.lock();
    this->writeq->push_back(std::move(asi));
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
bool async_send::on_read() {
    if (this->readq->empty()) {
        this->mtx.lock();
        std::swap(this->readq, this->writeq);
        this->mtx.unlock();
    }
    for (auto i = 0; i < 256 && !this->readq->empty(); ++i) {
        async_send::item &asi = this->readq->front();
        poll_desc *pd = this->poll->get_poll_desc(asi.fd);
        if (pd != nullptr && asi.seq == pd->seq) // 保证对象没有释放呢
            pd->eh->sync_ordered_send(asi.asb);
        else
            delete[] asi.asb.buf;
        
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
