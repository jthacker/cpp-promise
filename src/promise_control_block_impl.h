#pragma once

#include <functional>
#include <memory>

#include "lifecycle_listener_manager.h"
#include "promise_control_block.h"

namespace cpppromise {

template <typename T>
PromiseControlBlock<T>::PromiseControlBlock(std::string id) {
  if (LifecycleListenerManager::Get()) {
    p_listener = LifecycleListenerManager::Get()->OnPromiseCreated(id);
  }
}

template <typename T>
void PromiseControlBlock<T>::Resolve(T result) {
  std::unique_lock<std::mutex> lock(mu_);
  assert(!result_.has_value());
  result_ = result;
  NotifyDependents();
  if (p_listener) {
    p_listener->OnResolved();
  }
}

template <typename X>
template <typename Y>
std::shared_ptr<PromiseControlBlock<Y>> PromiseControlBlock<X>::Then(
    EventQueue *q, std::function<Y(X)> f, std::string id) {
  std::unique_lock<std::mutex> lock(mu_);
  auto pcb = std::make_shared<PromiseControlBlock<Y>>(id);
  Resolver<Y> resolver(pcb);
  dependents_.push_back([q, f, resolver, id](X value) mutable {
    q->AddTask([f, resolver, value]() mutable { resolver.Resolve(f(value)); },
               id);
    q->Release();
  });
  q->Take();
  if (result_.has_value()) {
    NotifyDependents();
  }
  return pcb;
}

template <typename T>
void PromiseControlBlock<T>::NotifyDependents() {
  for (auto d : dependents_) {
    d(result_.value());
  }
  dependents_.clear();
}

}  // namespace cpppromise
