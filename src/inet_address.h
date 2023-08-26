#ifndef INET_ADDRESS_H_
#define INET_ADDRESS_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h> // for strtol
#include <cstring>
#include <netdb.h>

class inet_address
{
public:
    inet_address() {
        ::memset(&this->inet_addr, 0, sizeof(this->inet_addr));
    }
    explicit inet_address(const inet_address &addr) {
        ::memcpy(&this->inet_addr, addr.get_addr(), sizeof(this->inet_addr)); 
    }
    inet_address(unsigned short port_number, const char *ip) {
        this->set(port_number, ip);
    }
    inline inet_address &operator =(const inet_address &addr) {
        if (this != &addr)
            std::memcpy(&this->inet_addr, addr.get_addr(), sizeof(this->inet_addr)); 
        return *this;
    }

    void set(const unsigned short port_number, const char *ip) {
        ::memset(&this->inet_addr, 0, sizeof(this->inet_addr));
        this->inet_addr.sin_family = AF_INET;
        this->inet_addr.sin_port   = ::htons(port_number);
        this->inet_addr.sin_addr.s_addr   = ::inet_addr(ip);
    }

    // return the "dotted decimal" Internet address
    const char *get_host_addr(char *addr, size_t addr_size) const {
        return ::inet_ntop(AF_INET,
            (void *)&(this->inet_addr.sin_addr.s_addr),
            addr,
            addr_size);
    }

    inline unsigned short get_port_number(void) const {
        return ntohs(this->inet_addr.sin_port);
    }

    // Return the 4-byte IP address, converting it into host byte order.
    inline unsigned int get_ip_address(void) const {
        return ntohl(size_t(this->inet_addr.sin_addr.s_addr));
    }

    inline void* get_addr(void) const { return (void*)&this->inet_addr; }

    inline int get_addr_size(void) const {
        return static_cast<int>(sizeof(this->inet_addr));
    }
private:
    sockaddr_in inet_addr;
};

#endif // INET_ADDRESS_H_
