#pragma once

#include <functional>
#include <memory>

#include "event_queue.h"
#include "promise_control_block.h"

namespace cpppromise {

// Class Promise is the fundamental unit of coordination between different
// threads of execution. A Promise is a simple value type -- it is intended to
// be passed freely by copy.
template <typename X>
class Promise {
 public:
  explicit Promise(std::shared_ptr<PromiseControlBlock<X>> pcb);

  template <typename Y>
  Promise<Y> Then(EventQueue *q, std::function<Y(X)> f, std::string id = "");

  template <typename Y>
  Promise<Y> Then(std::function<Y(X)> f, std::string id = "");

  template <typename Y>
  Promise<Y> Then(std::function<Y()> f, std::string id = "");

  Promise<Empty> Then(std::function<void(X)> f, std::string id = "");

  Promise<Empty> Then(std::function<void(void)> f, std::string id = "");

  // Wait for all promises to be resolved
  template <typename... Ys>
  static Promise<Empty> ResolveAll(std::string id, Promise<Ys>... promises);

 private:
  std::shared_ptr<PromiseControlBlock<X>> pcb_;
};

}  // namespace cpppromise
