#ifndef POLL_DESC_H_
#define POLL_DESC_H_

#include <mutex>
#include <unordered_map>

// Forward declarations
class ev_handler;
class poll_desc;

class poll_desc {
public:
    poll_desc() = default;
    poll_desc& operator=(const poll_desc &v) {
        this->fd = v.fd;
        this->events = v.events;
        this->seq = v.seq;
        this->eh = v.eh;
        return *this;
    }

    int fd = -1;
    uint32_t events = 0;
    int64_t seq = -1;
    ev_handler *eh = nullptr;
};

class poll_desc_map {
public:
    poll_desc_map() = delete;
    poll_desc_map(const int arr_size)
        : arr_size(arr_size),
        arr(new poll_desc[arr_size]()),
        map(1024)
    { }
    ~poll_desc_map() {
        if (this->arr != nullptr)
            delete[] this->arr;
        this->mtx.lock();
        for (const auto &kv : this->map)
            delete kv.second;
        
        this->mtx.unlock();
    }
public:
    inline poll_desc *new_one(const int i) {
        if (i < this->arr_size)
            return &(this->arr[i]);

        return new poll_desc();
    }

    inline poll_desc *load(const int i) {
        if (i < this->arr_size) {
            poll_desc *p = &(this->arr[i]);
            if (p->fd == -1)
                return nullptr;
            return p;
        }
        std::lock_guard<std::mutex> g(this->mtx);
        auto ret = this->map.find(i);
        if (ret != this->map.end())
            return ret->second;

        return nullptr;
    }

    inline void store(const int i, poll_desc *v) {
        if (i < this->arr_size)
            return;

        this->mtx.lock();
        this->map[i] = v;
        this->mtx.unlock();
    }

    void del(const int i) {
        if (i < this->arr_size) {
            poll_desc *p = &(this->arr[i]);
            p->events =  0;
            p->fd     = -1;
            p->seq    = -1;
            p->eh     = nullptr;
            return;
        }
        this->mtx.lock();
        auto ret = this->map.find(i);
        if (ret != this->map.end()) {
            delete ret->second;
            this->map.erase(ret);
        }
        this->mtx.unlock();
    }
private:
    int arr_size = 0;
    poll_desc *arr = nullptr;

    std::unordered_map<int, poll_desc *> map;
    std::mutex mtx;
};

#endif // POLL_DESC_H_
