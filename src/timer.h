#pragma once

#include <chrono>
#include <functional>

namespace cpppromise {

// A Timer is a singleton allowing clients to schedule tasks to be run
// at times in the future.
class Timer {
 public:
  using clock = std::chrono::steady_clock;

  // Return the current time according to the clock used by this Timer.
  virtual clock::time_point Now() = 0;

  // Schedule a task to execute at a time in the future. If the given time is
  // not in the future, the task will be executed as soon as possible, at some
  // arbitrary order compared to other tasks. Return an ID for this particular
  // future execution.
  virtual uint64_t Schedule(clock::time_point when,
                            std::function<void()> f) = 0;

  // Given an ID for a task execution, cancel the execution of the task. Return
  // true if that task had not yet been executed -- i.e., if the call to Cancel
  // actually resulted in the modification of the schedule of execution. Return
  // false if the ID is unknown or if the task has already been executed by the
  // time Cancel is called.
  virtual bool Cancel(uint64_t id) = 0;

  // Return the singleton Timer instance.
  static Timer* Get();
};

}  // namespace cpppromise
