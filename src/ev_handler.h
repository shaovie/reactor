#ifndef EV_HANDLER_H_
#define EV_HANDLER_H_

#include "socket.h"

#include <cstddef>
#include <sys/epoll.h>

// Forward declarations
class poller; 
class reactor; 
class timer_item; 
class async_send_buf; 

class ev_handler
{
    friend class poller;
    friend class reactor;
    friend class io_handle;
    friend class acceptor;
    friend class connector;
    friend class async_send;
    friend class timer_qheap;
public:
  enum {
    ev_read       = EPOLLIN  | EPOLLRDHUP,
    ev_write      = EPOLLOUT | EPOLLRDHUP,
    ev_accept     = EPOLLIN  | EPOLLRDHUP,
    ev_connect    = EPOLLIN  | EPOLLOUT | EPOLLRDHUP,
    ev_all        = ev_read|ev_write|ev_accept|ev_connect,
  };
  enum {
      err_connect_timeout = 1,
      err_connect_fail    = 2, // hostunreach, connrefused, connreset
  };
public:
  virtual ~ev_handler() {}

  virtual bool on_open()  { return false; }

  // the on_close(const int ev) will be called if below on_read/on_write return false. 
  virtual bool on_read()  { return false; }

  virtual bool on_write() { return false; }

  virtual void on_connect_fail(const int /*err*/) { }

  virtual bool on_timeout(const int64_t /*now*/) 	{ return false; }
  virtual void on_close() { }

  inline virtual int get_fd() const { return this->fd; }
  inline virtual void set_fd(const int v) { this->fd = v; }

  inline poller *get_poller() const { return this->poll; }
  inline reactor *get_reactor(void) const { return this->rct; }
  inline int64_t get_seq() const { return this->seq; }

  void destroy() {
      if (this->fd != -1) {
          socket::close(this->fd);
          this->fd = -1;
      }
      this->poll = nullptr;
      this->rct  = nullptr;
  }
private:
  virtual void sync_ordered_send(const async_send_buf &) { }
  inline void set_seq(const int64_t v) { this->seq = v; }

  inline void set_poller(poller *p) { this->poll = p; }

  inline void set_reactor(reactor *r) { this->rct = r; }

  inline timer_item *get_timer() { return this->ti; }
  inline void set_timer(timer_item *t) { this->ti = t; }
protected:
  ev_handler() = default;
private:
  int fd = -1;
  int64_t seq = -1;
  poller *poll = nullptr;
  reactor *rct = nullptr;
  timer_item *ti = nullptr;
};

#endif // EV_HANDLER_H_
