#ifndef EV_HANDLER_H_
#define EV_HANDLER_H_

#include <cstddef>

// Forward declarations
class reactor; 

class ev_handler
{
public:
  enum 
  {
    ev_null       = 0,
    ev_read       = 1L << 1,
    ev_write      = 1L << 2,
    ev_accept     = 1L << 3,
    ev_connect    = 1L << 5,
  };
public:
  virtual ~ev_handler() {}

  // the on_close(const int ev) will be called if below on_read/on_write return false. 
  virtual bool on_read()  { return false; }

  virtual bool on_write() { return false; }

  virtual bool on_timeout(const int64_t /*now*/) 	{ return false; }
  virtual void on_close(const int /*ev*/) { return -1; }

  virtual int get_fd() const { return -1; }
  virtual void set_fd(const int /*fd*/) { }

  void set_reactor(reactor *rc) { this->rct = r; }
  reactor *get_reactor(void) const { return this->rct; }
protected:
  ev_handler() = default;

private:
  int fd = -1;
  reactor *rct = nullptr;
};

#endif // EV_HANDLER_H_
