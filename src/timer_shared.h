#ifndef TIMER_THREAD_SAFE_H_
#define TIMER_THREAD_SAFE_H_

#include "reactor.h"
#include "ev_handler.h"
#include "timer_qheap.h"

#include <mutex>

// Forward declarations
class reactor;

// Thread-safe wrap
// This is a global timer, which is protected by a mutex and can only be called in the reactor instance

class timer_shared : public timer_qheap {
public:
    timer_shared(const int reserve): timer_qheap(reserve) { }
    virtual ~timer_shared() { }

    int schedule(ev_handler *eh, const int delay, const int interval) {
        std::lock_guard<std::mutex> g(this->mtx);
        return timer_qheap::schedule(eh, delay, interval);
    }
    void cancel(ev_handler *eh) {
        this->mtx.lock();
        timer_qheap::cancel(eh);
        this->mtx.unlock();
    }
    virtual bool on_read() {
        std::lock_guard<std::mutex> g(this->mtx);
        return timer_qheap::on_read();
    }
private:
    std::mutex mtx;
};

#endif // TIMER_THREAD_SAFE_H_
