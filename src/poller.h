#ifndef POLLER_H_
#define POLLER_H_

#include "timer_qheap.h"
#include "async_send.h"
#include "poll_desc.h"

#include <pthread.h>
#include <cstdint>
#include <atomic>
#include <map>

// Forward declarations
class options;
class ev_handler; 
class timer_qheap; 
class poll_desc_map;
class poll_sync_opt;
struct epoll_event; 

class poller {
    friend class reactor;
    friend class io_handle;
    friend class async_send;
    friend class poll_sync_opt;
public:
    poller() = default;

    int open(const options &opt);

    inline int schedule_timer(ev_handler *eh, const int delay, const int interval) {
        return this->timer->schedule(eh, delay, interval);
    }
    inline void cancel_timer(ev_handler *eh) { this->timer->cancel(eh); }

    int add(ev_handler *eh, const int fd, const uint32_t ev);

    int append(const int fd, const uint32_t ev);

    int remove(const int fd, const uint32_t ev);

    void run();
private:
    inline void push(async_send::item &&asi) { this->async_sendq->push(std::move(asi)); }
    poll_desc *get_poll_desc(const int fd) { return this->poll_descs->load(fd); }

private:
    void init_poll_sync_opt(const int t, void *arg);
    void do_poll_sync_opt(const int t, void *arg);
    void poll_cache_set(const int id, void *val, void (*free_func)(void *)) {
        auto itor = this->pcache.find(id);
        if (itor != this->pcache.end())
            free_func(itor->second);
        this->pcache[id] = val;
    }
    void *poll_cache_get(const int id) {
        auto itor = this->pcache.find(id);
        if (itor != this->pcache.end())
            return itor->second;
        return nullptr;
    }
private:
    void set_cpu_id(const int id) { this->cpu_id = id; }
    void set_cpu_affinity();
    void destroy();
private:
    int cpu_id = -1;
    int efd = -1;
    int ready_events_size = 0;
    int io_buf_size = 0;
    char *io_buf = nullptr;
    struct epoll_event *ready_events = nullptr;
    timer_qheap *timer = nullptr;
    async_send *async_sendq = nullptr;
    poll_desc_map *poll_descs = nullptr;
    poll_sync_opt *poll_sync_opterate = nullptr;
    pthread_t thread_id;
    std::atomic<int64_t> seq;
    std::map<int, void *> pcache;
};

#endif // POLLER_H_
