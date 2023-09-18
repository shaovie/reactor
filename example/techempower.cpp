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
const char httpheaders1[] = "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nServer: libreactor\r\nContent-Type: text/plain\r\nDate: ";
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

        if (this->content_length > 0) {
            int len = std::min(this->content_length, (int64_t)ret);
            this->content_length -= len;
            if (this->content_length > 0)
                return true;
            this->method = 0;
            buf = buf + len;
        }

        if (this->method == 0) {
            if (*buf == 'G') {
                this->method = 1;
                if (::strstr(buf, "\r\n\r\n") == nullptr)
                    return false;
            } else if (*buf == 'P') {
                buf[ret] = 0;
                this->method = 2;
                char *p = ::strcasestr(buf, "Content-Length: ");
                if (p == nullptr)
                    return false;
                this->content_length = strtoll(p + (sizeof("Content-Length: ") - 1), nullptr, 10);
                p = ::strstr(p + (sizeof("Content-Length: ") - 1), "\r\n\r\n");
                if (p == nullptr)
                    return false;
                this->content_length -= ret - (p + 4 - buf);
                if (this->content_length > 0)
                    return true;
            } else
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
private:
    int method = 0; // get:1 post/put:2
    int64_t content_length = 0;
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
    int poller_num = std::thread::hardware_concurrency();
    if (argc > 1)
        poller_num = atoi(argv[1]);

    signal(SIGPIPE ,SIG_IGN);

    reactor *accept_reactor = new reactor();
    conn_reactor = new reactor();
    opt.set_cpu_affinity  = false;
    opt.with_timer_shared = true;
    opt.poller_num = 1;
    if (accept_reactor->open(opt) != 0)
        ::exit(1);

    opt.reuse_addr = true;
    acceptor acc(accept_reactor, gen_http);
    if (acc.open(":8080", opt) != 0)
        ::exit(1);
    accept_reactor->run(false);

    opt.set_cpu_affinity  = true;
    opt.with_timer_shared = false;
    opt.poller_num = poller_num;
    if (conn_reactor->open(opt) != 0)
        ::exit(1);

    sync_date *sd = new sync_date();
    if (accept_reactor->schedule_timer(sd, 800, 1000) != 0)
        ::exit(1);
    sd->init();

    conn_reactor->run();
}
