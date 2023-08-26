#ifndef REACTOR_H_
#define REACTOR_H_

// Forward declarations
class poller; 
class ev_handler; 

class reactor {
public:
    reactor() = delete;
    reactor(const int n);

    int open(options opt);

    int add_ev_handler(ev_handler *eh, int fd, uint32_t events);

    int append_ev(int fd, uint32_t events);

    int remove_ev(int fd, uint32_t events);
    
    int run();
private:
    int     poller_num;
    poller *pollers;
};

#endif // REACTOR_H_
