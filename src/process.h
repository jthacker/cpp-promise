#pragma once

// #include <string>

#include "empty.h"
#include "promise.h"
#include "resolver.h"
#include "schedule.h"
#include "string.h"

namespace cpppromise {

class Process {
 public:
  Process(std::string id = "");

  virtual ~Process() = default;

  virtual void Join() { q_.Join(); }

 protected:
  template <typename T>
  Promise<T> Enqueue(std::function<T(void)> f, std::string id = "");

  Promise<Empty> Enqueue(std::function<void(void)> f, std::string id = "");

  template <typename T>
  std::pair<Promise<T>, Resolver<T>> CreateResolver(std::string id = "");

  template <typename T>
  Promise<T> EnqueueWithResolver(std::function<void(Resolver<T>)>,
                                 std::string id = "");

  void Finish() { q_.Finish(); }

  Schedule DoPeriodically(std::function<bool()> f,
                          std::chrono::nanoseconds interval,
                          std::string id = "");

  Schedule DoPeriodically(std::function<Promise<bool>()> f,
                          std::chrono::nanoseconds interval,
                          std::string id = "");

 private:
  EventQueue q_;
};

}  // namespace cpppromise
