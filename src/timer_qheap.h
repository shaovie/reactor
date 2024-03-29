#ifndef TIMER_QHEAP_H_
#define TIMER_QHEAP_H_

#include "ev_handler.h"

#include <cstdint>
#include <vector>

// Forward declarations
class timer_item; 

// Quad heap implements
// This is an unlocked timer that can only be used within the poller.

class timer_item {
public:
    timer_item() = default;

    int32_t interval = 0;
    int64_t expire_at = 0;
    ev_handler *eh = nullptr;
};

class timer_qheap : public ev_handler {
public:
    timer_qheap(const int reserve);

    virtual ~timer_qheap();
public:
    int open();

    virtual int get_fd() const { return this->tfd; }
    virtual void set_fd(const int ) { }

    // it's ok return 0, failed return -1
    int schedule(ev_handler *eh, const int delay, const int interval);

    void cancel(ev_handler *eh);

    int64_t handle_expired(int64_t now);

    virtual bool on_read();
private:
    inline void insert(timer_item *);

    timer_item* pop_min(int64_t now, int64_t &delta);

    void shift_up(int index);

    void shift_down(int index);

    void adjust_timerfd(int64_t delay);
private:
    int tfd = -1;
    int64_t timerfd_settime = 0;
    std::vector<timer_item *> qheap;
};

#endif // TIMER_QHEAP_H_
