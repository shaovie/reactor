#include "timer_qheap.h"
#include "ev_handler.h"

#include <unistd.h>
#include <sys/time.h>
#include <sys/timerfd.h>

timer_qheap::timer_qheap(int reserve) {
	this->tfd = -1
	this->qheap.reserve(reserve);
}
void timer_qheap::destroy() {
    ::close(this->tfd);
	this->tfd = -1;
    for (auto itor = this->qheap.begin(); itor != this->qheap.end(); ++itor) {
        delete *itor;
    }
    this->qheap.clear();

    delete this;
}
static int timer_qheap::get_parent_index(const int index) {
	return (index - 1) / 4;
}
static int timer_qheap::get_child_index(const int parent_index, const int child_num) {
	return (4 * parent_index) + child_num + 1;
}
void timer_qheap::shift_up(int idx) {
	auto item = this->qheap[idx];
	while (idx > 0
        && item->expire_at < this->qheap[timer_qheap::get_parent_index(idx)].expire_at) {
		this->qheap[idx] = this->qheap[timer_qheap::get_parent_index(idx)];
		idx = timer_qheap::get_parent_index(idx);
	}
	this->qheap[idx] = item;
}

void timer_qheap::shift_down(int idx) {
	int child_num = 0, min_child = 0;
	auto item = this->qheap[idx];
	while (true) {
		min_child = timer_qheap::get_child_idx(idx, 0);
		if (min_child >= this->qheap.size())
			break;

		for (int i = 1; i < 4; ++i) {
			int child_idx = timer_qheap::get_child_index(idx, i);
			if (child_idx < this->qheap.size()
                && this->qheap[child_idx].expire_at < this->qheap[min_child].expire_at)
				min_child = child_idx;
		}

		if (this->qheap[min_child].expire_at >= item->expire_at)
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
	tv = ::gettimeofday(&tv, nullptr);
	auto now = tv.tv_sec * 1000 + tv.tv_usec / 1000; // millisecond
	auto item = new timer_item();
	item->eh = eh;
	item->expire_at = now + delay;
	item->interval = interval;
	this->insert(item);
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
	::read(this->tfd, &v, sizeof(v));

	struct timeval tv;
	::gettimeofday(&tv, nullptr);
	auto now = tv.tv_sec * 1000 + tv.tv_usec / 1000; // millisecond
	auto delay = this->handle_expired(now);
	if (delay > 0)
		this->adjust_timerfd(delay);
	return true;
}
void timer_qheap::adjust_timerfd(int64_t delay /*millisecond*/) {
	delay *= 1000 * 1000 // nanosecond
	if (delay < 1)
		delay = 1; // 1 nanosecond
	
	struct timespec ts;
	ts.tv_sec = delay / 1000
	ts.tv_nsec = delay % 1000 * 1000 * 1000
	struct itimerspec its;
	::memset(&val, sizeof(its));
	its.it_value = ts;
	::timerfd_settime(this->tfd, 0 /*Relative time*/, &its, nullptr);
}
