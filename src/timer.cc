#include "src/cpp_common/cpppromise/timer.h"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <thread>

namespace cpppromise {

struct Task {
  uint64_t id;
  std::function<void()> callback;
};

class TimerImpl : public Timer {
 public:
  TimerImpl() : run_(true), id_counter_(0) {
    t_ = std::thread([this]() {
      while (true) {
        std::optional<Task> this_task;

        {
          std::unique_lock<std::mutex> lock(mu_);
          if (!run_) {
            break;
          } else if (tasks_.empty()) {
            cond_.wait(lock);
          } else {
            auto t = tasks_.begin()->first;
            if (t <= Now()) {
              this_task = tasks_.begin()->second;
              tasks_.erase(t);
            } else {
              cond_.wait_for(lock, t - Now());
            }
          }
        }

        if (this_task.has_value()) {
          this_task.value().callback();
        }
      }
    });
  }

  ~TimerImpl() {
    {
      std::unique_lock<std::mutex> lock(mu_);
      run_ = false;
      cond_.notify_one();
    }
    t_.join();
  }

  clock::time_point Now() override { return clock::now(); }

  uint64_t Schedule(clock::time_point when, std::function<void()> f) override {
    std::unique_lock<std::mutex> lock(mu_);
    uint64_t id = id_counter_++;
    tasks_.insert({when, {id, f}});
    cond_.notify_one();
    return id;
  }

  virtual bool Cancel(uint64_t id) {
    std::unique_lock<std::mutex> lock(mu_);
    for (auto i = tasks_.cbegin(); i != tasks_.cend();) {
      if (i->second.id == id) {
        tasks_.erase(i);
        cond_.notify_one();
        return true;
      } else {
        ++i;
      }
    }
    return false;
  }

 private:
  bool run_;
  uint64_t id_counter_;
  std::mutex mu_;
  std::thread t_;
  std::condition_variable cond_;
  std::map<clock::time_point, Task> tasks_;
};

static TimerImpl timer;

Timer* Timer::Get() { return &timer; }

}  // namespace cpppromise
