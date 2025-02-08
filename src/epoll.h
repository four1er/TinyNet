#pragma once

#include "base.h"
#include "poller.h"
#include <sys/epoll.h>
#include <unistd.h>

namespace NNet {
class TSocket;
class TFileHandle;
class TEpoll : public TPollerBase {
public:
  using TSocket = NNet::TSocket;
  using TFileHandle = NNet::TFileHandle;

  TEpoll();
  ~TEpoll();

  void Poll();

private:
  int fd_;

  std::vector<THandlePair> in_events_;
  std::vector<epoll_event> out_events_;
};
} // namespace NNet
