#ifndef POLL_SYNC_OPT_H_
#define POLL_SYNC_OPT_H_

#include "ev_handler.h"
#include "ringq.h"

#include <mutex>
#include <atomic>

// Forward declarations
class poller; 

class poll_sync_opt : public ev_handler {
public:
    enum {
        sync_cache_t = 1,
    };
    class sync_cache {
    public:
        sync_cache() = default;
        int id = 0;
        void *value = nullptr;
        void (*free_func)(void *) = nullptr; // 负责释放上次一的value
    };

    class opt_arg {
    public:
        opt_arg() = default;
        opt_arg(const int t, void *a) : type(t), arg(a) { }
        opt_arg& operator=(opt_arg &&v) {
            this->type = v.type;
            this->arg = v.arg;
            return *this;
        }
        opt_arg& operator=(const opt_arg &v) {
            this->type = v.type;
            this->arg = v.arg;
            return *this;
        }

        int type = 0;
        void *arg = nullptr;
    };

    poll_sync_opt() = delete;
    poll_sync_opt(const int init_size) {
        this->readq  = new ringq<poll_sync_opt::opt_arg>(init_size);
        this->writeq = new ringq<poll_sync_opt::opt_arg>(init_size);
    }
    ~poll_sync_opt();

    int open(poller *);

    virtual bool on_read();

    void init(poll_sync_opt::opt_arg &&arg);
    void push(poll_sync_opt::opt_arg &&arg);
private:
    void do_sync(const poll_sync_opt::opt_arg &arg);

    int efd = -1;
    std::mutex mtx;
    std::atomic<int> notified;
    ringq<poll_sync_opt::opt_arg> *readq  = nullptr;
    ringq<poll_sync_opt::opt_arg> *writeq = nullptr;
};

#endif // POLL_SYNC_OPT_H_
