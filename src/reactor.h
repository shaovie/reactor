#ifndef REACTOR_H_
#define REACTOR_H_

#include <cstdint>

// Forward declarations
class poller;
class options;
class ev_handler; 

class reactor {
public:
    reactor() = delete;
    reactor(const int n);

    int open(options *opt);

    int add_ev_handler(ev_handler *eh, const int fd, const uint32_t events);

    int append_ev(const int fd, const uint32_t events);

    int remove_ev(const int fd, const uint32_t events);
    
    void run();
private:
    int     poller_num;
    poller *pollers;
};

#endif // REACTOR_H_
