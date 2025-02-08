#include "epoll.h"
#include <sys/epoll.h>

namespace NNet {

static constexpr int epoll_flags = EPOLL_CLOEXEC;
static constexpr int invalid_handle = -1;

TEpoll::TEpoll() : fd_(epoll_create1(epoll_flags)) {
    if (fd_ == invalid_handle) {
        throw std::runtime_error("epoll_create1 failed");
    }
}

TEpoll::~TEpoll() {
    if (fd_ != invalid_handle) {
        close(fd_);
    }
}

void TEpoll::Poll() {
    auto ts = GetTimeout();
    if (static_cast<int>(in_events_.size()) <= max_fd_) {
        in_events_.resize(max_fd_ + 1);
    }

    for (const auto &ch : changes_) {
        int fd = ch.Fd;
        auto &ev = in_events_[fd];
        epoll_event eev = {};
        eev.data.fd = fd;
        bool change = false;
        bool new_ev = false;
        if (ch.Handle) {
            new_ev = !!ev.Read && !!ev.Write && !!ev.RHup;
            if (ch.Type & TEvent::READ) {
                eev.events |= EPOLLIN;
                change |= ev.Read != ch.Handle;
                ev.Read = ch.Handle;
            }
            if (ch.Type & TEvent::WRITE) {
                eev.events |= EPOLLOUT;
                change |= ev.Write != ch.Handle;
                ev.Write = ch.Handle;
            }
            if (ch.Type & TEvent::RHUP) {
                eev.events |= EPOLLRDHUP;
                change |= ev.RHup != ch.Handle;
                ev.RHup = ch.Handle;
            }
        } else {
            if (ch.Type & TEvent::READ) {
                change |= !!ev.Read;
                ev.Read = {};
            }
            if (ch.Type & TEvent::WRITE) {
                change |= !!ev.Write;
                ev.Write = {};
            }
            if (ev.Read) {
                eev.events |= EPOLLIN;
            }
            if (ev.Write) {
                eev.events |= EPOLLOUT;
            }
            if (ev.RHup) {
                eev.events |= EPOLLRDHUP;
            }
        }
    }
}

}  // namespace NNet
