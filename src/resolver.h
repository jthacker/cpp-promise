#pragma once

#include "promise_control_block.h"

namespace cpppromise {

// A Resolver is a special gadget that represents the "supply side" of a
// Promise.
template <typename T>
class Resolver {
 public:
  explicit Resolver(std::shared_ptr<PromiseControlBlock<T>> pcb);

  void Resolve(T result);

 private:
  std::shared_ptr<PromiseControlBlock<T>> pcb_;
};

}  // namespace cpppromise
