#ifndef EV_HANDLER_H_
#define EV_HANDLER_H_

#include <cstddef>
#include <sys/epoll.h>

// Forward declarations
class poller; 
class reactor; 

class ev_handler
{
public:
  enum 
  {
    ev_null       = 0,
    ev_read       = EPOLLIN | EPOLLRDHUP,
    ev_write      = EPOLLOUT | EPOLLRDHUP,
    ev_accept     = EPOLLIN | EPOLLRDHUP,
    ev_connect    = EPOLLIN|EPOLLOUT|EPOLLRDHUP,
    ev_all        = ev_read|ev_write|ev_accept|ev_connect,
  };
public:
  virtual ~ev_handler() {}

  // the on_close(const int ev) will be called if below on_read/on_write return false. 
  virtual bool on_read()  { return false; }

  virtual bool on_write() { return false; }

  virtual bool on_timeout(const int64_t /*now*/) 	{ return false; }
  virtual void on_close() { }

  int get_fd() const { return this->fd; }
  void set_fd(const int v) { this->fd = v; }

  poller *get_poller() const { return this->poll; }
  void set_poller(poller *p) { this->poll = p; }

  void set_reactor(reactor *r) { this->rct = r; }
  reactor *get_reactor(void) const { return this->rct; }
protected:
  ev_handler() = default;

private:
  int fd = -1;
  poller *poll = nullptr;
  reactor *rct = nullptr;
};

#endif // EV_HANDLER_H_
