#include "timer_qheap.h"
#include "ev_handler.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timerfd.h>

#define get_parent_index(idx) (((idx) - 1) / 4)
#define get_child_index(parent_index, child_num) ((4 * (parent_index)) + (child_num) + 1)

timer_qheap::timer_qheap(const int reserve) {
    this->qheap.reserve(reserve);
}
timer_qheap::~timer_qheap() {
    if (this->tfd != -1) {
        ::close(this->tfd);
        this->tfd = -1;
    }
    for (const auto &v : this->qheap)
        delete v;
    this->qheap.clear();
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
    struct timespec ts;
    ts.tv_sec = delay / 1000;
    ts.tv_nsec = delay % 1000 * 1000 * 1000;
    if (delay < 1)
        ts.tv_nsec = 1;
    struct itimerspec its;
    ::memset(&its, 0, sizeof(its));
    its.it_value = ts;
    ::timerfd_settime(this->tfd, 0 /*Relative time*/, &its, nullptr);
}
int timer_qheap::schedule(ev_handler *eh, const int delay, const int interval) {
    if (eh == nullptr)
        return -1;

    struct timeval tv;
    ::gettimeofday(&tv, nullptr);
    int64_t now = int64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000; // millisecond
    auto item = new timer_item();
    item->eh = eh;
    item->expire_at = now + delay;
    item->interval = interval;
    eh->set_timer(item);
    this->insert(item);

    auto min = this->qheap[0];
    if (min->expire_at != this->timerfd_settime) {
        this->adjust_timerfd(min->expire_at - now);
        this->timerfd_settime = min->expire_at;
    }
    return 0;
}
void timer_qheap::cancel(ev_handler *eh) {
    auto item = eh->get_timer();
    if (item == nullptr)
        return;
    item->eh = nullptr;
    item->expire_at = 1;
    eh->set_timer(nullptr);
}
void timer_qheap::shift_up(int idx) {
    auto item = this->qheap[idx];
    while (idx > 0
        && item->expire_at < this->qheap[get_parent_index(idx)]->expire_at) {
        this->qheap[idx] = this->qheap[get_parent_index(idx)];
        idx = get_parent_index(idx);
    }
    this->qheap[idx] = item;
}

void timer_qheap::shift_down(int idx) {
    int min_child = 0;
    auto item = this->qheap[idx];
    while (true) {
        min_child = get_child_index(idx, 0);
        if (min_child >= static_cast<int>(this->qheap.size()))
            break;

        for (int i = 1; i < 4; ++i) {
            int child_idx = get_child_index(idx, i);
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
int timer_qheap::handle_expired(int64_t now) {
    if (this->qheap.empty())
        return -1;

    int delta = 0;
    while (true) {
        auto item = this->pop_min(now, delta);
        if (item == nullptr)
            break;

        if (item->eh == nullptr) { // canceled
            delete item;
            continue;
        }
        if (item->eh->on_timeout(now) == true && item->interval > 0) {
            item->expire_at = now + item->interval;
            this->insert(item);
        } else {
            item->eh->set_timer(nullptr);
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
