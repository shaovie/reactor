#ifndef POLLER_H_
#define POLLER_H_

#include <cstdint>

// Forward declarations
class ev_handler; 
class timer_qheap; 
class poll_desc_map;
struct epoll_event; 

class poller {
public:
    poller() = default;
    virtual ~poller();

    int open(const int timer_init_size);

    int schedule_timer(ev_handler *eh, const int delay, const int interval);

    int add(ev_handler *eh, const int fd, const uint32_t ev);

    int append(const int fd, const uint32_t ev);

    int remove(const int fd, const uint32_t ev);

    void run();
private:
    void destroy();
private:
    int efd = -1;
    struct epoll_event *ready_events = nullptr;
    timer_qheap *timer = nullptr;
    poll_desc_map *poll_descs = nullptr;
};

#endif // POLLER_H_
