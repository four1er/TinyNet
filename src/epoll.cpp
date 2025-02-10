#include <sys/epoll.h>
#include "epoll.h"

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
    std::cout << "Epoll::polling..." << std::endl;
    std::cout << "in_events_.size: " << in_events_.size() << std::endl;
    std::cout << "out_events_.size: " << out_events_.size() << std::endl;
    auto ts = GetTimeout();
    std::cout << "poll timeout: " << ts.tv_sec << "s," << ts.tv_nsec / 1e6 << "ms" << std::endl;
    std::cout << "max_fd_: " << max_fd_ << std::endl;
    if (static_cast<int>(in_events_.size()) <= max_fd_) {
        in_events_.resize(max_fd_ + 1);
    }

    for (const auto &ch : changes_) {
        int fd = ch.Fd;
        std::cout << "current fd: " << fd << ", type: " << ch.Type << std::endl;
        auto &ev = in_events_[fd];
        epoll_event eev = {};
        eev.data.fd = fd;
        bool change = false;
        bool new_ev = false;
        if (ch.Handle) {
            std::cout << "fd: " << fd << ", has handle" << std::endl;
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
            std::cout << "fd: " << fd << ", no handle" << std::endl;
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
        std::cout << "fd: " << fd << ", is new_ev: " << new_ev << ", changd? " << change
                  << std::endl;
        if (new_ev) {
            std::cout << "epoll_ctl add: " << fd << std::endl;
            if (epoll_ctl(fd_, EPOLL_CTL_ADD, fd, &eev) < 0) {
                throw std::runtime_error("epoll_ctl failed");
            }
        } else if (!eev.events) {
            std::cout << "epoll_ctl del: " << fd << std::endl;
            if (epoll_ctl(fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
                if (!(errno == EBADF ||
                      errno == ENOENT)) {  // closed descriptor after TSocket -> close
                    throw std::system_error(errno, std::generic_category(), "epoll_ctl");
                }
            }
        } else if (change) {
            std::cout << "epoll_ctl mod: " << fd << std::endl;
            if (epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &eev) < 0) {
                if (errno == ENOENT) {
                    if (epoll_ctl(fd_, EPOLL_CTL_ADD, fd, &eev) < 0) {
                        throw std::system_error(errno, std::generic_category(), "epoll_ctl");
                    }
                } else {
                    throw std::system_error(errno, std::generic_category(), "epoll_ctl");
                }
            }
        }
    }

    Reset();
    std::cout << "changes size: " << changes_.size() << std::endl;
    std::cout << "ready size: " << ready_events_.size() << std::endl;
    out_events_.resize(std::max<size_t>(1, in_events_.size()));
    std::cout << "in_events_.size: " << in_events_.size() << std::endl;
    std::cout << "out_events_.size: " << out_events_.size() << std::endl;

    int nfds;
    // if ((nfds = epoll_pwait2(fd_, &out_events_[0], out_events_.size(), &ts, nullptr)) < 0) {
    //     if (errno == EINTR) {
    //         return;
    //     }
    //     throw std::system_error(errno, std::generic_category(), "epoll_pwait");
    // }

    if ((nfds = epoll_pwait(fd_, &out_events_[0], out_events_.size(), 100, nullptr)) < 0) {
        if (errno == EINTR) {
            return;
        }
        throw std::system_error(errno, std::generic_category(), "epoll_pwait");
    }

    std::cout << "epoll_pwait done, nfds size: " << nfds << std::endl;

    for (int i = 0; i < nfds; ++i) {
        int fd = out_events_[i].data.fd;
        std::cout << "fd: " << fd << ", events: " << out_events_[i].events << std::endl;
        auto ev = in_events_[fd];
        if (out_events_[i].events & EPOLLIN) {
            ready_events_.emplace_back(TEvent{fd, TEvent::READ, ev.Read});
            ev.Read = {};
        }
        if (out_events_[i].events & EPOLLOUT) {
            ready_events_.emplace_back(TEvent{fd, TEvent::WRITE, ev.Write});
            ev.Write = {};
        }
        if (out_events_[i].events & EPOLLHUP) {
            if (ev.Read) {
                ready_events_.emplace_back(TEvent{fd, TEvent::READ, ev.Read});
            }
            if (ev.Write) {
                ready_events_.emplace_back(TEvent{fd, TEvent::WRITE, ev.Write});
            }
        }
        if (out_events_[i].events & EPOLLRDHUP) {
            if (ev.Read) {
                ready_events_.emplace_back(TEvent{fd, TEvent::READ, ev.Read});
            }
            if (ev.Write) {
                ready_events_.emplace_back(TEvent{fd, TEvent::WRITE, ev.Write});
            }
            if (ev.RHup) {
                ready_events_.emplace_back(TEvent{fd, TEvent::RHUP, ev.RHup});
            }
        }
    }

    ProcessTimers();
}

}  // namespace NNet
