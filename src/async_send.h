#ifndef ASYNC_SEND_H_
#define ASYNC_SEND_H_

#include "ringq.h"
#include "io_handle.h"
#include "ev_handler.h"

#include <mutex>
#include <atomic>

// Forward declarations
class poller; 

class async_send : public ev_handler {
public:
    class item {
    public:
        item() = default;
        item(const int fd, const int64_t seq, async_send_buf &&asb)
            : fd(fd), seq(seq), asb(asb) { }
        item& operator=(item &&v) {
            this->fd = v.fd;
            this->seq = v.seq;
            this->asb = std::move(v.asb);
            return *this;
        }
        item& operator=(const item &v) {
            this->fd = v.fd;
            this->eh = v.eh;
            this->asb = v.asb;
            return *this;
        }

        int fd = -1;
        int64_t seq = -1;
        ev_handler *eh = nullptr;
        async_send_buf asb;
    };
    async_send() = delete;
    async_send(const int init_size) {
        this->readq  = new ringq<async_send::item>(init_size);
        this->writeq = new ringq<async_send::item>(init_size);
    }
    ~async_send();

    int open(poller *);

    virtual bool on_read();

    void push(async_send::item &&asi);
private:
    int efd = -1;
    std::mutex mtx;
    std::atomic<int> notified;
    ringq<async_send::item> *readq  = nullptr;
    ringq<async_send::item> *writeq = nullptr;
};

#endif // ASYNC_SEND_H_
