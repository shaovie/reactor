#include "../src/reactor.h"
#include "../src/io_handle.h"
#include "../src/acceptor.h"
#include "../src/options.h"
#include "../src/poll_sync_opt.h"

#include <string>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <thread>

reactor *conn_reactor = nullptr;
const char httpheaders1[] = "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nServer: goev\r\nContent-Type: text/plain\r\nDate: ";
const char httpheaders2[] = "\r\nContent-Length: 13\r\n\r\nHello, World!";

const int pcache_data_t = 1;

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

        int writen = 0;
        ::memcpy(buf, httpheaders1, sizeof(httpheaders1)-1);
        writen += sizeof(httpheaders1)-1;

        char *date = (char *)this->poll_cache_get(pcache_data_t);
        ret = ::strlen(date);
        ::memcpy(buf + writen, date, ret);
        writen += ret;

        ret = sizeof(httpheaders2)-1;
        ::memcpy(buf + writen, httpheaders2, ret);
        writen += ret;

        this->send(buf, writen);
        return true;
    }
    virtual void on_close() {
        this->destroy();
    }
};
ev_handler *gen_http() {
    return new http();
}
void release_dates(void *p) {
    delete[] (char *)p;
}
class sync_date : public ev_handler {
public:
    void init() {
        struct timeval now;
        gettimeofday(&now, NULL);
        auto args = this->build_args(now.tv_sec);
        conn_reactor->init_poll_sync_opt(poll_sync_opt::sync_cache_t, args);
    }
    virtual bool on_timeout(const int64_t now) {
        printf("ok\n");
        auto args = this->build_args(now);
        conn_reactor->poll_sync_opt(poll_sync_opt::sync_cache_t, args);
        return true;
    }
    void ** build_args(const int64_t now) {
        struct tm tmv;
        auto nowt = (time_t)now;
        ::localtime_r(&nowt, &tmv);
        char dates[32] = {0};
        ::strftime(dates, 32, "%a, %d %b %Y %H:%M:%S GMT", &tmv);
        void **args = new void *[conn_reactor->get_poller_num()];
        for (int i = 0; i < conn_reactor->get_poller_num(); ++i) {
            char *ds = new char[32]{0};
            ::strcpy(ds, dates);
            poll_sync_opt::sync_cache *arg = new poll_sync_opt::sync_cache();
            arg->id = pcache_data_t;
            arg->value = ds;
            arg->free_func = release_dates;
            args[i] = arg;
        }
        return args;
    }
};
int main (int argc, char *argv[]) {
    options opt;
    if (argc > 1)
        opt.poller_num = atoi(argv[1]);

    signal(SIGPIPE ,SIG_IGN);

    opt.set_cpu_affinity = false;
    reactor *accept_reactor = new reactor();
    opt.with_timer_shared = true;
    if (accept_reactor->open(opt) != 0)
        ::exit(1);

    opt.set_cpu_affinity = true;
    conn_reactor = new reactor();
    opt.with_timer_shared = false;
    if (conn_reactor->open(opt) != 0)
        ::exit(1);

    sync_date *sd = new sync_date();
    accept_reactor->schedule_timer(sd, 800, 1000);
    sd->init();

    opt.reuse_addr = true;
    acceptor acc(accept_reactor, gen_http);
    if (acc.open(":8080", opt) != 0)
        ::exit(1);
    accept_reactor->run(false);

    conn_reactor->run();
}
