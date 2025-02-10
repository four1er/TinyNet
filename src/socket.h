#pragma once

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <coroutine>
#include <optional>
#include <stdexcept>
#include <variant>

#include "address.h"
#include "base.h"
#include "poller.h"

namespace NNet {
template <typename T>
class TSocketBase;

template <>
class TSocketBase<void> {
 public:
    TPollerBase* Poller() { return poller_; }

 protected:
    TSocketBase() = default;
    TSocketBase(TPollerBase& poller, int domain, int type);
    TSocketBase(int fd, TPollerBase& poller);

    int Create(int domain, int type);

    int Setup(int s);

    TPollerBase* poller_ = nullptr;
    int fd_ = -1;
};

template <typename TSockOps>
class TSocketBase : public TSocketBase<void> {
 public:
    auto ReadSome(void* buf, size_t size) {
        struct TAwaitableRead : TAwaitable<TAwaitableRead> {
            void run() { this->ret = TSockOps::read(this->fd, this->b, this->s); }

            void await_suspend(std::coroutine_handle<> handle) {
                this->poller->AddRead(this->fd, handle);
            }
        };
        return TAwaitableRead{poller_, fd_, buf, size};
    }

    auto ReadSomeYield(void* buf, size_t size) {
        struct TAwaitableRead : TAwaitable<TAwaitableRead> {
            bool await_ready() { return (this->ready = false); }
            void await_suspend(std::coroutine_handle<> handle) {
                this->poller->AddRead(this->fd, handle);
            }
            void run() { this->ret = TSockOps::read(this->fd, this->b, this->s); }
        };
        return TAwaitableRead{poller_, fd_, buf, size};
    }

    auto WriteSome(const void* buf, size_t size) {
        struct TAwaitableWrite : public TAwaitable<TAwaitableWrite> {
            void run() { this->ret = TSockOps::write(this->fd, this->b, this->s); }

            void await_suspend(std::coroutine_handle<> h) { this->poller->AddWrite(this->fd, h); }
        };
        return TAwaitableWrite{poller_, fd_, const_cast<void*>(buf), size};
    }

    auto WriteSomeYield(const void* buf, size_t size) {
        struct TAwaitableWrite : public TAwaitable<TAwaitableWrite> {
            bool await_ready() { return (this->ready = false); }

            void run() { this->ret = TSockOps::write(this->fd, this->b, this->s); }

            void await_suspend(std::coroutine_handle<> h) { this->poller->AddWrite(this->fd, h); }
        };
        return TAwaitableWrite{poller_, fd_, const_cast<void*>(buf), size};
    }

    auto Monitor() {
        struct TAwaitableClose : public TAwaitable<TAwaitableClose> {
            void run() { this->ret = true; }
            void await_suspend(std::coroutine_handle<> h) {
                this->poller->AddRemoteHup(this->fd, h);
            }
        };
        return TAwaitableClose{poller_, fd_};
    }

    void Close() {
        if (fd_ >= 0) {
            TSockOps::close(fd_);
            poller_->RemoveEvent(fd_);
            fd_ = -1;
        }
    }

 protected:
    TSocketBase(TPollerBase& poller, int domain, int type)
        : TSocketBase<void>(poller, domain, type) {}
    TSocketBase(int fd, TPollerBase& poller) : TSocketBase<void>(fd, poller) {}
    TSocketBase() = default;
    TSocketBase(const TSocketBase&) = delete;
    TSocketBase& operator=(TSocketBase&) const = delete;

    template <typename T>
    struct TAwaitable {
        bool await_ready() {
            SafeRun();
            return (ready = (ret >= 0));
        }

        int await_resume() {
            if (!ready) {
                SafeRun();
            }
            return ret;
        }

        void SafeRun() {
            ((T*)this)->run();
            if (ret < 0 && !(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                throw std::system_error(errno, std::generic_category(), "Socket operation failed");
            }
        }

        TPollerBase* poller = nullptr;
        int fd = -1;
        void* b = nullptr;
        size_t s = 0;
        int ret = -1;
        bool ready = false;
    };
};

class TSockOps {
 public:
    static auto read(int fd, void* buf, size_t count) {
        return ::recv(fd, static_cast<char*>(buf), count, 0);
    }

    static auto write(int fd, const void* buf, size_t count) {
        return ::send(fd, static_cast<const char*>(buf), count, 0);
    }

    static auto close(int fd) {
        if (fd >= 0) {
            ::close(fd);
        }
    }
};

class TSocket : public TSocketBase<TSockOps> {
 public:
    using TPoller = TPollerBase;

    TSocket() = default;

    TSocket(TPoller& poller, int domain, int type = SOCK_STREAM)
        : TSocketBase(poller, domain, type) {}

    TSocket(const TAddress& addr, int fd, TPoller& poller)
        : TSocketBase(fd, poller), remote_addr_(addr) {}

    TSocket(TSocket&& other) { *this = std::move(other); }

    TSocket& operator=(TSocket&& other) {
        if (this != &other) {
            Close();
            poller_ = other.poller_;
            remote_addr_ = other.remote_addr_;
            local_addr_ = other.local_addr_;
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    auto Connect(const TAddress& addr, TTime deadline = TTime::max()) {
        std::cout << "Connect to " << addr.ToString() << std::endl;
        if (remote_addr_.has_value()) {
            throw std::runtime_error("Already connected");
        }
        remote_addr_ = addr;
        struct TAwaitable {
            bool await_ready() {
                std::cout << "try to connect, fd: " << fd << std::endl;
                int ret = connect(fd, addr.first, addr.second);
                std::cout << "connect ret: " << ret << " , errno: " << errno << std::endl;
                if (ret < 0 && !(errno == EINTR || errno == EAGAIN || errno == EINPROGRESS)) {
                    throw std::system_error(errno, std::generic_category(), "Connect failed");
                }
                return ret >= 0;
            }

            void await_suspend(std::coroutine_handle<> h) {
                std::cout << "suspend on Connecting, fd: " << fd << std::endl;
                poller->AddWrite(fd, h);
                if (deadline != TTime::max()) {
                    time_id = poller->AddTimer(deadline, h);
                    std::cout << "add timer, fd: " << fd << std::endl;
                }
            }

            void await_resume() {
                if (deadline != TTime::max() && poller->RemoveTimer(time_id, deadline)) {
                    throw std::runtime_error("Connect timeout");
                }
            }

            TPollerBase* poller;
            int fd;
            std::pair<const sockaddr*, int> addr;
            TTime deadline;
            unsigned time_id = 0;
        };

        return TAwaitable{poller_, fd_, remote_addr_->RawAddr(), deadline};
    }

    auto Accept() {
        struct TAwaitable {
            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<> h) {
                std::cout << "suspend on Accepting, "
                          << "fd: " << fd << std::endl;
                poller->AddRead(fd, h);
                std::cout << "suspend done" << std::endl;
            }
            TSocket await_resume() {
                std::cout << "resume on Accepting, fd: " << fd << std::endl;
                char clientaddr[sizeof(sockaddr_in6)];
                socklen_t addrlen = sizeof(sockaddr_in6);

                int clientfd = accept(fd, reinterpret_cast<sockaddr*>(clientaddr), &addrlen);
                if (clientfd < 0) {
                    throw std::system_error(errno, std::generic_category(), "Accept failed");
                }
                return TSocket{TAddress{reinterpret_cast<sockaddr*>(clientaddr), addrlen}, clientfd,
                               *poller};
            }

            TPollerBase* poller;
            int fd;
        };
        return TAwaitable{poller_, fd_};
    }

    void Bind(const TAddress& addr);

    void Listen(int backlog = 128);

    const std::optional<TAddress>& RemoteAddr() const { return remote_addr_; }

    const std::optional<TAddress>& LocalAddr() const { return local_addr_; }

 protected:
    std::optional<TAddress> local_addr_;
    std::optional<TAddress> remote_addr_;
};

}  // namespace NNet
