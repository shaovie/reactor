#ifndef REACTOR_H_
#define REACTOR_H_

#include <cstdint>

// Forward declarations
class poller;
class options;
class ev_handler; 

class reactor {
public:
    reactor() = default;

    //= run
    int open(const options &opt);
    void run(const bool join = true);
    int get_poller_num() const { return this->poller_num; }

    //= event
    int add_ev_handler(ev_handler *eh, const int fd, const uint32_t events);
    int append_ev(const int fd, const uint32_t events);
    int remove_ev(const int fd, const uint32_t events);
    
    //= poll
    void init_poll_sync_opt(const int t, void* arg[]);
    void poll_sync_opt(const int t, void* arg[]);
private:
    int     poller_num = 0;
    poller *pollers = nullptr;
};

#endif // REACTOR_H_
