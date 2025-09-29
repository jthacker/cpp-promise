#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>

#include "event_queue.h"
#include "promise_listener.h"

namespace cpppromise {

template <typename T>
class PromiseControlBlock {
 public:
  PromiseControlBlock(std::string id);

  void Resolve(T result);

  template <typename Y>
  std::shared_ptr<PromiseControlBlock<Y>> Then(EventQueue *q,
                                               std::function<Y(T)> f,
                                               std::string id);

 private:
  void NotifyDependents();

  std::optional<T> result_;
  std::mutex mu_;
  std::vector<std::function<void(T)>> dependents_;
  std::shared_ptr<PromiseListener> p_listener;
};

}  // namespace cpppromise
