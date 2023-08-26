#ifndef EV_HANDLER_H_
#define EV_HANDLER_H_

#include <cstddef>
#include <sys/epoll.h>

// Forward declarations
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

  virtual int get_fd() const { return -1; }
  virtual void set_fd(const int /*fd*/) { }

  void set_reactor(reactor *r) { this->rct = r; }
  reactor *get_reactor(void) const { return this->rct; }
protected:
  ev_handler() = default;

private:
  int fd = -1;
  reactor *rct = nullptr;
};

#endif // EV_HANDLER_H_
