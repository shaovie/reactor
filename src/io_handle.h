#ifndef IO_HANDLE_H_
#define IO_HANDLE_H_

#include "ev_handler.h"

class async_send_buf {
public:
    async_send_buf() = default;
    async_send_buf(char *bf, const int l): len(l), buf(bf) { }
    async_send_buf(async_send_buf &&v): sendn(v.sendn), len(v.len), buf(v.buf) { }
    async_send_buf(const async_send_buf &v): sendn(v.sendn), len(v.len), buf(v.buf) { }
    async_send_buf& operator=(async_send_buf &&v) {
        this->buf = v.buf;
        this->len = v.len;
        this->sendn = v.sendn;
        return *this;
    }
    async_send_buf& operator=(const async_send_buf &v) {
        this->buf = v.buf;
        this->len = v.len;
        this->sendn = v.sendn;
        return *this;
    }

    int sendn = 0;
    int len   = 0;
    char *buf = nullptr;
};

class io_handle : public ev_handler {
    friend class async_send;
public:
    io_handle() = default;

    int recv(void* &buff);
    int send(const void *buff, const size_t len);

    virtual bool on_write();

    void destroy();
private:
    virtual void sync_ordered_send(const async_send_buf &asb);

    int async_send(const void *buff, const size_t len);

    int async_send_buf_size = 0;
    ringq<async_send_buf> async_send_buf_q{2};
};

#endif // IO_HANDLE_H_
