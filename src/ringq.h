#ifndef RINGQ_H_
#define RINGQ_H_

#include <utility>

template<class item_t>
class ringq {
public:
    ringq() = delete;
    ~ringq() { if (this->rq != nullptr) delete[] this->rq; }

    ringq(const int init_size): size(init_size) {
        this->rq = new item_t[init_size]();
    }

    bool empty() { return this->len == 0; }
    bool full()  { return this->len == this->size; }
    int  length() const { return this->len;  }

    inline void push_back(const item_t &v) {
        if (this->full())
            this->grow();
        this->rq[this->tail] = v;
        this->tail = (this->tail + 1) % this->size;
        ++(this->len);
    }
    inline void push_back(item_t &&v) {
        if (this->full())
            this->grow();
        this->rq[this->tail] = std::move(v);
        this->tail = (this->tail + 1) % this->size;
        ++(this->len);
    }
    inline void pop_front() {
        if (this->empty())
            return;

        this->rq[this->head] = std::move(item_t()); // clear
        this->head = (this->head + 1) % this->size;
        --(this->len);
    }
    inline item_t &front() { return this->rq[this->head]; }
private:
    inline void grow() {
        int new_size = this->size * 2;
        if (new_size == 0)
            new_size = 2;
        item_t *newrq = new item_t[new_size]();

        for (int i = 0; i < this->len; ++i)
            newrq[i] = this->rq[(this->head + i) % this->size];
        delete[] this->rq;
        this->rq = newrq;
        this->size = new_size;
        this->head = 0;
        this->tail = this->len;
    }
private:
    int size = 0;
    int head = 0;
    int tail = 0;
    int len  = 0;
    item_t *rq = nullptr;
};

#endif // RINGQ_H_
