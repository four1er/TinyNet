#pragma once

#include <assert.h>
#include <chrono>
#include <coroutine>
#include <iostream>
#include <map>
#include <queue>
#include <vector>

#include "base.h"

namespace NNet {
class TPollerBase {
 public:
    TPollerBase() = default;
    TPollerBase(const TPollerBase &) = delete;
    TPollerBase &operator=(const TPollerBase &) = delete;

    unsigned AddTimer(TTime deadline, THandle handle) {
        timers_.emplace(TTimer{deadline, time_id_, handle});
        std::cout << "timer id: " << time_id_ << std::endl;
        return time_id_++;
    }

    unsigned RemoveTimer(unsigned timer_id, TTime deadline) {
        bool fired = timer_id == last_fired_timer_;
        if (!fired) {
            timers_.emplace(TTimer{deadline, timer_id, THandle{}});
        }
        return fired;
    }

    void AddRead(int fd, THandle handle) {
        max_fd_ = std::max(max_fd_, fd);
        std::cout << "current max fd: " << max_fd_ << ", input fd: " << fd << std::endl;
        changes_.emplace_back(std::move(TEvent{fd, TEvent::READ, handle}));
        std::cout << "current changes size: " << changes_.size() << std::endl;
    }

    void AddWrite(int fd, THandle handle) {
        max_fd_ = std::max(max_fd_, fd);
        changes_.emplace_back(std::move(TEvent{fd, TEvent::WRITE, handle}));
    }

    void AddRemoteHup(int fd, THandle handle) {
        max_fd_ = std::max(max_fd_, fd);
        changes_.emplace_back(std::move(TEvent{fd, TEvent::RHUP, handle}));
    }

    void RemoveEvent(int fd) {
        max_fd_ = std::max(max_fd_, fd);
        changes_.emplace_back(
            std::move(TEvent{fd, TEvent::READ | TEvent::WRITE | TEvent::RHUP, THandle{}}));
    }

    void RemoveEvent(THandle /*h */) {}

    auto Sleep(TTime until) {
        struct TAwaitableSleep {
            TAwaitableSleep(TPollerBase *poller, TTime n) : poller_(poller), n_(n) {}
            ~TAwaitableSleep() {
                if (poller_) {
                    poller_->RemoveEvent(timer_id_);
                }
            }

            TAwaitableSleep(TAwaitableSleep &&other) : poller_(other.poller_), n_(other.n_) {
                other.poller_ = nullptr;
            }

            TAwaitableSleep(const TAwaitableSleep &) = delete;
            TAwaitableSleep &operator=(const TAwaitableSleep &) = delete;

            bool await_ready() { return false; }

            void await_suspend(std::coroutine_handle<> handle) {
                std::cout << "Sleep..." << std::endl;
                timer_id_ = poller_->AddTimer(n_, handle);
            }

            void await_resume() { poller_ = nullptr; }

            TPollerBase *poller_;
            TTime n_;
            unsigned timer_id_ = 0;
        };

        return TAwaitableSleep(this, until);
    }

    template <typename Rep, typename Period>
    auto Sleep(std::chrono::duration<Rep, Period> duration) {
        return Sleep(TClock::now() + duration);
    }

    auto Yield() { return Sleep(TTime{}); }

    void Wakeup(TEvent &&change) {
        auto index = changes_.size();
        change.Handle.resume();
        if (change.Fd >= 0) {
            bool matched = false;
            for (; index < changes_.size(); ++index) {
                if (changes_[index].Match(change)) {
                    matched = true;
                }
            }
            if (!matched) {
                change.Handle = {};
                changes_.emplace_back(std::move(change));
            }
        }
    }

    void WakeupReadyHandles() {
        for (auto &&ev : ready_events_) {
            Wakeup(std::move(ev));
        }
    }

    auto TimersSize() const { return timers_.size(); }

    timespec GetTimeout() const {
        return timers_.empty() ? max_duration_ts_
               : timers_.top().Deadline == TTime{}
                   ? timespec{0, 0}
                   : GetTimespec(TClock::now(), timers_.top().Deadline, max_duration_);
    }

    static constexpr timespec GetMaxDuration(std::chrono::milliseconds duration) {
        auto p1 = std::chrono::duration_cast<std::chrono::seconds>(duration);
        auto p2 = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - p1);
        timespec ts;
        ts.tv_sec = p1.count();
        ts.tv_nsec = p2.count();
        return ts;
    }

    void SetMaxDuration(std::chrono::milliseconds max_duration) {
        max_duration_ = max_duration;
        max_duration_ts_ = GetMaxDuration(max_duration_);
    }

    void Reset() {
        ready_events_.clear();
        changes_.clear();
        max_fd_ = 0;
    }

    void ProcessTimers() {
        auto now = TClock::now();
        bool first = true;
        unsigned prev_id = 0;
        while (!timers_.empty() && timers_.top().Deadline <= now) {
            TTimer timer = timers_.top();
            std::cout << "timer id: " << timer.Id << std::endl;
            timers_.pop();
            if ((first || prev_id != timer.Id) && timer.Handle) {
                last_fired_timer_ = timer.Id;
                timer.Handle.resume();
            }
            first = false;
            prev_id = timer.Id;
        }
        last_timers_process_time_ = now;
    }

 protected:
    int max_fd_;                          // max file descriptor in use
    std::vector<TEvent> changes_;         // events to be processed (registered events)
    std::vector<TEvent> ready_events_;    // events ready to wake up their coroutines
    unsigned time_id_ = 0;                // counter for generating unique timer ids
    std::priority_queue<TTimer> timers_;  // timers to be processed
    TTime last_timers_process_time_;      // time of the last timers processing
    unsigned last_fired_timer_ = -1;      // id of the last fired timer
    std::chrono::milliseconds max_duration_ = std::chrono::milliseconds(100);  // max poll duration

    timespec max_duration_ts_ = GetMaxDuration(max_duration_);  // max poll duration in timespec
};
}  // namespace NNet
