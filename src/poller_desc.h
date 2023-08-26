
// Forward declarations
class poller_desc;
class std::map<int, poller_desc *>;
class std::mutex;

class poller_desc {
public:
    int fd = -1;
	uint32_t events = 0;
	ev_handler *eh = nullptr;
};

class poller_desc_map {
public:
    poller_desc_map() = delete;
    poller_desc_map(const int arr_size) {
        this->arr_size = arr_size;
        this->arr = new poller_desc[arr_size]();
    }

public:
    inline poller_desc *new_one(const int i);
    inline poller_desc *load(const int i);
    inline poller_desc *store(const int i, poller_desc *v);
    inline void del(const int i);
}

private:
	int arr_size;
	poller_desc *arr;

    std::map<int, poller_desc *> map;
	std::mutex mtx;
}
