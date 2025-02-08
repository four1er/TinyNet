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
class PollerBase {
public:
private:
  int max_fd_;                  // max file descriptor in use
  std::vector<TEvent> changes_; // events to be processed (registered events)
  std::vector<TEvent> ready_events_; // events ready to wake up their coroutines
  unsigned time_id_ = 0;             // counter for generating unique timer ids
  std::priority_queue<TTimer> timers_; // timers to be processed
  TTime last_timers_process_time_;     // time of the last timers processing
  unsigned last_fired_timer_ = -1;     // id of the last fired timer
  std::chrono::milliseconds max_duration_ =
      std::chrono::milliseconds(100); // max poll duration
};
} // namespace NNet
