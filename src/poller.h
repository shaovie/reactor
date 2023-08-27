#ifndef POLLER_H_
#define POLLER_H_

#include <pthread.h>
#include <cstdint>

// Forward declarations
class options;
class ev_handler; 
class timer_qheap; 
class poll_desc_map;
struct epoll_event; 

class poller {
    friend class reactor;
public:
    poller() = default;
    virtual ~poller();

    int open(const options &opt);

    int schedule_timer(ev_handler *eh, const int delay, const int interval);
    void cancel_timer(ev_handler *eh);

    int add(ev_handler *eh, const int fd, const uint32_t ev);

    int append(const int fd, const uint32_t ev);

    int remove(const int fd, const uint32_t ev);

    void run();
private:
    void set_cpu_id(const int id) { this->cpu_id = id; }
    void set_cpu_affinity();
    void destroy();
private:
    int cpu_id = -1;
    int efd = -1;
    int ready_events_size = 0;
    void *io_buf = nullptr;
    struct epoll_event *ready_events = nullptr;
    timer_qheap *timer = nullptr;
    poll_desc_map *poll_descs = nullptr;
    pthread_t thread_id;
};

#endif // POLLER_H_
