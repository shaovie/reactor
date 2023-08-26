#include "timer_qheap.h"
#include "ev_handler.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timerfd.h>

timer_qheap::timer_qheap(poller *p, int reserve) {
	this->tfd = -1;
    this->poll = p;
	this->qheap.reserve(reserve);
}
timer_qheap::~timer_qheap() {
    if (this->tfd != -1) {
        ::close(this->tfd);
        this->tfd = -1;
    }
    for (auto itor = this->qheap.begin(); itor != this->qheap.end(); ++itor)
        delete *itor;
    
    this->qheap.clear();
}
void timer_qheap::destroy() {
    delete this;
}
int timer_qheap::open() {
    int fd = ::timerfd_create(CLOCK_BOOTTIME, TFD_NONBLOCK|TFD_CLOEXEC);
    if (fd == -1) {
        fprintf(stderr, "reactor: timerfd_create fail! %s\n", strerror(errno));
        return -1;
    }
    this->tfd = fd;
    return 0;
}
void timer_qheap::adjust_timerfd(int64_t delay /*millisecond*/) {
	delay *= 1000 * 1000; // nanosecond
	if (delay < 1)
		delay = 1; // 1 nanosecond
	
	struct timespec ts;
	ts.tv_sec = delay / 1000;
	ts.tv_nsec = delay % 1000 * 1000 * 1000;
	struct itimerspec its;
	::memset(&its, 0, sizeof(its));
	its.it_value = ts;
	::timerfd_settime(this->tfd, 0 /*Relative time*/, &its, nullptr);
}
int timer_qheap::get_parent_index(const int index) {
	return (index - 1) / 4;
}
int timer_qheap::get_child_index(const int parent_index, const int child_num) {
	return (4 * parent_index) + child_num + 1;
}
void timer_qheap::shift_up(int idx) {
	auto item = this->qheap[idx];
	while (idx > 0
        && item->expire_at < this->qheap[timer_qheap::get_parent_index(idx)]->expire_at) {
		this->qheap[idx] = this->qheap[timer_qheap::get_parent_index(idx)];
		idx = timer_qheap::get_parent_index(idx);
	}
	this->qheap[idx] = item;
}

void timer_qheap::shift_down(int idx) {
	int min_child = 0;
	auto item = this->qheap[idx];
	while (true) {
		min_child = timer_qheap::get_child_index(idx, 0);
		if (min_child >= static_cast<int>(this->qheap.size()))
			break;

		for (int i = 1; i < 4; ++i) {
			int child_idx = timer_qheap::get_child_index(idx, i);
			if (child_idx < static_cast<int>(this->qheap.size())
                && this->qheap[child_idx]->expire_at < this->qheap[min_child]->expire_at)
				min_child = child_idx;
		}

		if (this->qheap[min_child]->expire_at >= item->expire_at)
			break;

		this->qheap[idx] = this->qheap[min_child];
		idx = min_child;
	}
	this->qheap[idx] = item;
}
void timer_qheap::insert(timer_item *item) {
	this->qheap.push_back(item);
	this->shift_up(this->qheap.size() - 1);
}

timer_item* timer_qheap::pop_min(int64_t now, int &delta) {
	if (this->qheap.empty())
		return nullptr;

	auto min = this->qheap[0];
	delta = min->expire_at - now;
	if (delta > 0)
		return nullptr;
	
	delta = 0;
	this->qheap[0] = this->qheap.back();
	this->qheap.pop_back();
	this->shift_down(0);

	return min;
}
int timer_qheap::schedule(ev_handler *eh, const int delay, const int interval) {
	if (eh == nullptr)
		return -1;

	struct timeval tv;
	::gettimeofday(&tv, nullptr);
	auto now = tv.tv_sec * 1000 + tv.tv_usec / 1000; // millisecond
	auto item = new timer_item();
	item->eh = eh;
	item->expire_at = now + delay;
	item->interval = interval;
	this->insert(item);

	auto min = this->qheap[0];
    if (min->expire_at != this->timerfd_settime) {
        this->adjust_timerfd(min->expire_at - now);
        this->timerfd_settime = min->expire_at;
    }
    return 0;
}
int timer_qheap::handle_expired(int64_t now) {
	if (this->qheap.empty())
		return -1;
	
	int delta = 0;
	while (true) {
		auto item = this->pop_min(now, delta);
		if (item == nullptr)
			break;
		
		if (item->eh->on_timeout(now) == true && item->interval > 0) {
			item->expire_at = now + item->interval;
			this->insert(item);
		} else {
			delete item;
		}
	}
	return delta;
}
bool timer_qheap::on_read() {
	int64_t v = 0;
    int ret = 0;
    do {
        ret = ::read(this->tfd, &v, sizeof(v));
        if (ret == -1 && errno == EINTR)
            continue;
    } while (false); 

	struct timeval tv;
	::gettimeofday(&tv, nullptr);
	auto now = tv.tv_sec * 1000 + tv.tv_usec / 1000; // millisecond
	auto delay = this->handle_expired(now);
	if (delay > 0)
		this->adjust_timerfd(delay);
	return true;
}
