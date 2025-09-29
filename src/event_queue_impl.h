#pragma once

#include "event_queue.h"
#include "promise_control_block.h"

namespace cpppromise {

template <typename T>
Promise<T> EventQueue::Enqueue(std::function<T()> f, std::string id) {
  std::shared_ptr<PromiseControlBlock<T>> pcb =
      std::make_shared<PromiseControlBlock<T>>(id);
  AddTask([this, f, pcb]() { pcb->Resolve(f()); }, id);
  return Promise<T>(pcb);
}

template <typename T>
std::pair<Promise<T>, Resolver<T>> EventQueue::CreateResolver(std::string id) {
  auto pcb = std::make_shared<PromiseControlBlock<T>>(id);
  return {Promise<T>(pcb), Resolver<T>(pcb)};
}

template <typename T>
Promise<T> EventQueue::CreateResolvedPromise(T val, std::string id) {
  auto pair = CreateResolver<T>(id);
  pair.second.Resolve(val);
  return pair.first;
}

template <typename T>
Promise<T> EventQueue::EnqueueWithResolver(
    std::function<void(Resolver<T>)> resolve, std::string id) {
  auto pair = CreateResolver<T>(id);
  AddTask([resolve, resolver = pair.second]() { resolve(resolver); }, id);
  return pair.first;
}

}  // namespace cpppromise
