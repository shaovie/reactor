#include "poll_desc.h"

#include <mutex>

poll_desc_map::~poll_desc_map() {
    if (this->arr != nullptr)
        delete[] this->arr;
    this->mtx.lock();
    for (auto itor = this->map.begin(); itor != this->map.end();) {
        delete itor->second;
        this->map.erase(itor++);
    }
    this->mtx.unlock();
}
poll_desc *poll_desc_map::new_one(const int i) {
    if (i < this->arr_size)
        return &(this->arr[i]);

    return new poll_desc();
}
poll_desc *poll_desc_map::load(const int i) {
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
void poll_desc_map::store(const int i, poll_desc *v) {
    if (i < this->arr_size)
        return;

    this->mtx.lock();
    this->map[i] = v;
    this->mtx.unlock();
}
void poll_desc_map::del(const int i) {
    if (i < this->arr_size) {
        poll_desc *p = &(this->arr[i]);
        p->events =  0;
        p->fd     = -1;
        p->seq    = -1;
        p->eh     = nullptr;
        return;
    }
    std::lock_guard<std::mutex> g(this->mtx);
    auto ret = this->map.find(i);
    if (ret != this->map.end()) {
        delete ret->second;
        this->map.erase(ret);
    }
}
