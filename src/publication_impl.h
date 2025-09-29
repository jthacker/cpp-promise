#pragma once

#include <memory>

#include "publication.h"
#include "subscription_control_block.h"
#include "topic.h"

namespace cpppromise {

template <typename T>
Subscription<T> Publication<T>::Subscribe(std::function<void(T)> listener,
                                          std::string id) {
  assert(EventQueue::Get() != nullptr);
  auto scb = std::make_shared<SubscriptionControlBlock<T>>();
  scb->topic = topic_;
  scb->q = EventQueue::Get();
  scb->listener = listener;
  scb->id = id;
  auto trig = std::make_shared<SubscriptionUnsubscribeTrigger<T>>(scb);
  topic_->Add(scb);
  return Subscription(trig);
}

}  // namespace cpppromise