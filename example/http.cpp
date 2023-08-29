#include "../src/reactor.h"
#include "../src/io_handle.h"
#include "../src/acceptor.h"
#include "../src/options.h"

#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

reactor *conn_reactor = nullptr;
const char httpheaders[] = "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nServer: goev\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!";

class http : public io_handle {
public:
    virtual bool on_open() {
        // add_ev_handler 尽量放在最后, (on_open 和o_read可能不在一个线程)
        if (conn_reactor->add_ev_handler(this, this->get_fd(), ev_handler::ev_read) != 0) {
            fprintf(stderr, "add new conn to poller fail! %s\n", strerror(errno));
            return false;
        }
        return true;
    }
    virtual bool on_read() {
        char *buf = nullptr;
        int ret = this->recv(buf);
        if (ret == 0) // closed
            return false;
        else if (ret < 0)
            return true;

        if (::strstr(buf, "\r\n\r\n") == nullptr) {
            // invliad msg
            return false;
        }

        int buf_len = sizeof(httpheaders)-1;
        ::memcpy(buf, httpheaders, buf_len);
        this->send(buf, buf_len);
        return true;
    }
    virtual void on_close() {
        this->destroy();
    }
};
ev_handler *gen_http() {
    return new http();
}
int main (int argc, char *argv[]) {
    options opt;
    opt.set_cpu_affinity = true;
    opt.poller_num *= 2;
    if (argc > 1)
        opt.poller_num = atoi(argv[1]);

    conn_reactor = new reactor();
    if (conn_reactor->open(opt) != 0)
        ::exit(1);

    opt.reuse_addr = true;
    acceptor acc(conn_reactor, gen_http);
    if (acc.open(":8080", opt) != 0)
        ::exit(1);
    conn_reactor->run();
}
