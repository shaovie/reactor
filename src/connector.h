#ifndef CONNECTOR_H_
#define CONNECTOR_H_

#include "ev_handler.h"

// Forward declarations
class reactor;
class inet_address;

class connector : public ev_handler
{
    friend class in_progress_connect;
public:
    connector() = delete;
    connector(reactor *r) { this->set_reactor(r); }

    // timeout is milliseconds e.g. 200ms;
    int connect(ev_handler *eh, const inet_address &addr,
        const int timeout, const size_t rcvbuf_size = 0);
protected:
    int nblock_connect(ev_handler *eh, const int fd, const int timeout);
};

#endif // CONNECTOR_H_
