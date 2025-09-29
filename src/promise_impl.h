#pragma once

#include "promise.h"
#include "resolver.h"

namespace cpppromise {

template <typename X>
Promise<X>::Promise(std::shared_ptr<PromiseControlBlock<X>> pcb) : pcb_(pcb) {}

template <typename X>
template <typename Y>
Promise<Y> Promise<X>::Then(EventQueue *q, std::function<Y(X)> f,
                            std::string id) {
  return Promise<Y>(pcb_->Then(q, f, id));
}

template <typename X>
template <typename Y>
Promise<Y> Promise<X>::Then(std::function<Y(X)> f, std::string id) {
  assert(EventQueue::Get() != nullptr);
  return Then(EventQueue::Get(), f, id);
}

template <typename X>
template <typename Y>
Promise<Y> Promise<X>::Then(std::function<Y()> f, std::string id) {
  return Then<Y>([f](X) { return f(); }, id);
}

template <typename X>
Promise<Empty> Promise<X>::Then(std::function<void(X)> f, std::string id) {
  return Then<Empty>(
      [f](X x) {
        f(x);
        return Empty{};
      },
      id);
}

template <typename X>
Promise<Empty> Promise<X>::Then(std::function<void()> f, std::string id) {
  return Then(
      [f](X) {
        f();
        return Empty{};
      },
      id);
}

template <>
template <typename... Ys>
inline Promise<Empty> Promise<Empty>::ResolveAll(std::string id,
                                                 Promise<Ys>... promises) {
  auto pair = EventQueue::CreateResolver<Empty>(id);
  std::shared_ptr<int> count = std::make_shared<int>(sizeof...(promises));

  (promises.template Then(
       [count, resolver = pair.second]() mutable {
         if (--(*count) == 0) {
           resolver.Resolve(Empty());
         }
       },
       ""),
   ...);

  return pair.first;
}

}  // namespace cpppromise
