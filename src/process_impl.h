#pragma once

#include "process.h"

namespace cpppromise {

template <typename T>
Promise<T> Process::Enqueue(std::function<T(void)> f, std::string id) {
  return q_.Enqueue(f, id);
}

template <typename T>
std::pair<Promise<T>, Resolver<T>> Process::CreateResolver(std::string id) {
  return q_.CreateResolver<T>(id);
}

template <typename T>
Promise<T> Process::EnqueueWithResolver(
    std::function<void(Resolver<T>)> resolve, std::string id) {
  return q_.EnqueueWithResolver(resolve, id);
}

}  // namespace cpppromise
