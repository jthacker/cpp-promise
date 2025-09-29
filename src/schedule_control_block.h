#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

#include "empty.h"
#include "event_queue.h"
#include "promise.h"
#include "resolver.h"

namespace cpppromise {

class ScheduleControlBlock
    : public std::enable_shared_from_this<ScheduleControlBlock> {
 public:
  ScheduleControlBlock(EventQueue* q, std::function<Promise<bool>()> f,
                       std::chrono::nanoseconds interval, std::string id,
                       Resolver<Empty> done);
  ~ScheduleControlBlock();

  void Start();
  void Cancel();

 private:
  void ScheduleNextRun();
  void TimerCallback();
  void Finish();

  std::mutex mu_;
  EventQueue* q_;
  std::function<Promise<bool>()> f_;
  std::chrono::nanoseconds interval_;
  std::string id_;
  bool running_;
  Timer::clock::time_point scheduled_run_time_;
  std::optional<uint64_t> current_timer_;
  Resolver<Empty> done_;
};

}  // namespace cpppromise