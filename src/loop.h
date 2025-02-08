#pragma once

namespace NNet {

template <typename TPoller>
class TLoop {
 public:
    void Loop() {
        while (running_) {
            Step();
        }
    }

    void Stop() { running_ = false; }

    void Step() {
        poller_.Poller();
        poller_.WakeipReadyHnadles();
    }

    TPoller& Poller() { return poller_; }

 private:
    TPoller poller_;
    bool running_ = false;
};
}  // namespace NNet