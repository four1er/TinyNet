
#include "socket.h"

namespace NNet {

TSocketBase<void>::TSocketBase(TPollerBase& poller, int domain, int type)
    : poller_(&poller), fd_(Create(domain, type)) {}

TSocketBase<void>::TSocketBase(int fd, TPollerBase& poller) : poller_(&poller), fd_(Setup(fd)) {}

int TSocketBase<void>::Create(int domain, int type) {
    auto s = socket(domain, type, 0);
    if (s == static_cast<decltype(s)>(-1)) {
        throw std::system_error(errno, std::generic_category(), "Socket creation failed");
    }
    return Setup(s);
}

int TSocketBase<void>::Setup(int s) {
    int value;
    socklen_t len = sizeof(value);
    bool is_socket = false;
    if (getsockopt(s, SOL_SOCKET, SO_TYPE, &value, &len) == 0) {
        value = 1;
        is_socket = true;
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "Socket option SO_REUSEADDR failed");
        }
    }
    auto flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        throw std::system_error(errno, std::generic_category(), "Socket flags get failed");
    }
    if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw std::system_error(errno, std::generic_category(), "Socket flags set failed");
    }
    return s;
}

void TSocket::Bind(const TAddress& addr) {
    if (local_addr_.has_value()) {
        throw std::runtime_error("Already bound");
    }
    local_addr_ = addr;
    auto [rawaddr, len] = local_addr_->RawAddr();
    int optval = 1;
    socklen_t optlen = sizeof(optval);
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, optlen) < 0) {
        throw std::system_error(errno, std::generic_category(), "setsockopt");
    }
    if (bind(fd_, rawaddr, len) < 0) {
        throw std::system_error(errno, std::generic_category(), "bind");
    }
}

void TSocket::Listen(int backlog) {
    if (listen(fd_, backlog) < 0) {
        throw std::system_error(errno, std::generic_category(), "listen");
    }
}

}  // namespace NNet