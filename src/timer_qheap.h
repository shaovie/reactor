#ifndef TIMER_QHEAP_H_
#define TIMER_QHEAP_H_

#include "ev_handler.h"

#include <cstdint>
#include <vector>

// Forward declarations
class poller; 
class ev_handler;
class timer_item; 

// Quad heap implements

class timer_item {
public:
	timer_item() = default;

	int32_t interval = 0;
	int64_t expire_at = 0;
	ev_handler *eh = nullptr;
};

class timer_qheap : public ev_handler {
public:
	timer_qheap(poller *p, int reserve);
	timer_qheap(const timer_qheap &) = delete;
    virtual ~timer_qheap();
public:
    int timerfd() const { return this->tfd; }

	// it's ok return 0, failed return -1
	int schedule(ev_handler *eh, const int delay, const int interval);

    int handle_expired(int64_t now);

	virtual bool on_read();

    void destroy();
private:
    inline void insert(timer_item *);

	timer_item* pop_min(int64_t now, int &delta);

    void shift_up(int index);

    void shift_down(int index);

    void adjust_timerfd(int64_t delay);
private:
    static inline int get_parent_index(const int index);

    static inline int get_child_index(const int parent_index, const int child_num);
private:
	int tfd = -1;
    poller *poll = nullptr;
    std::vector<timer_item *> qheap;
};

#endif // TIMER_QHEAP_H_
