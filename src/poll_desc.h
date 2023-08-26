#ifndef POLL_DESC_H_
#define POLL_DESC_H_

#include <map>
#include <mutex>

// Forward declarations
class ev_handler;
class poll_desc;

class poll_desc {
public:
    poll_desc() = default;

    int fd = -1;
	uint32_t events = 0;
	ev_handler *eh = nullptr;
};

class poll_desc_map {
public:
    poll_desc_map() = delete;
    poll_desc_map(const int arr_size) {
        this->arr_size = arr_size;
        this->arr = new poll_desc[arr_size]();
    }
public:
    poll_desc *new_one(const int i);

    poll_desc *load(const int i);

    void store(const int i, poll_desc *v);

    inline void del(const int i);
private:
	int arr_size;
	poll_desc *arr;

    std::map<int, poll_desc *> map;
	std::mutex mtx;
};

#endif // POLL_DESC_H_
