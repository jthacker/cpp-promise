#pragma once

#include <functional>
#include <mutex>
#include <string>

#include "event_queue.h"
#include "topic.h"

namespace cpppromise {

template <typename T>
struct SubscriptionControlBlock {
  std::mutex mu;
  Topic<T> *topic;
  EventQueue *q;
  std::function<void(T)> listener;
  std::string id;
};

}  // namespace cpppromise