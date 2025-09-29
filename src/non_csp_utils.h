// Utils to interact with CSP process from non-csp thread

#pragma once

#include <cassert>

#include "src/cpp_common/cpppromise/cpppromise.h"
#include "src/cpp_common/cpppromise/cpppromise_stream.h"

// This header contains helper functions to interact with CSP thread from
// non-csp thread and should NEVER be used between two csp threads.
namespace cpppromise {

// Helper function to get promise result in the non-csp thread
template <typename T>
T Get(Promise<T> promise) {
  // Ensure this is never called from a CSP thread
  assert(EventQueue::Get() == nullptr);
  EventQueue q;
  T ret;

  q.Enqueue([&]() { promise.template Then([&](T res) { ret = res; }); });

  q.Finish();
  q.Join();

  return ret;
}

template <typename T>
T Get(std::function<Promise<T>()> async_func) {
  // Ensure this is never called from a CSP thread
  assert(EventQueue::Get() == nullptr);
  EventQueue q;
  T ret;

  q.Enqueue([&]() { async_func().template Then([&](T res) { ret = res; }); });

  q.Finish();
  q.Join();

  return ret;
}

// Helper function to manage subscription synchronously in non-csp
// thread Caller thread will be blocked until listener returns false. Listener
// will be executed in a different thread, aka an event queue.
template <typename T>
void SubscribeAndWait(Publication<T> publication,
                      std::function<bool(T data)> listener) {
  // Ensure this is never called from a CSP thread
  assert(EventQueue::Get() == nullptr);
  EventQueue q;

  std::condition_variable cv;
  std::mutex mu;
  bool done = false;

  std::optional<Subscription<T>> subscription;

  {
    q.Enqueue([&]() {
      subscription = publication.Subscribe([&](T data) {
        if (!listener(data)) {
          subscription.value().Unsubscribe();
          std::unique_lock<std::mutex> lock(mu);
          done = true;
          cv.notify_one();
        }
      });
    });

    std::unique_lock<std::mutex> lock(mu);
    cv.wait(lock, [&] { return done; });
  }

  q.Finish();
  q.Join();
}

}  // namespace cpppromise
