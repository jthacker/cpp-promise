#pragma once

#include <memory>

#include "subscription_unsubscribe_trigger.h"

namespace cpppromise {

template <typename T>
SubscriptionUnsubscribeTrigger<T>::SubscriptionUnsubscribeTrigger(
    std::shared_ptr<SubscriptionControlBlock<T>> block)
    : block_(block) {}

template <typename T>
SubscriptionUnsubscribeTrigger<T>::~SubscriptionUnsubscribeTrigger() {
  Unsubscribe();
}

template <typename T>
void SubscriptionUnsubscribeTrigger<T>::Unsubscribe() {
  std::shared_ptr<SubscriptionControlBlock<T>> deleted_block;
  Topic<T>* topic;

  {
    std::unique_lock<std::mutex> lock(block_->mu);
    if (block_->topic != nullptr) {
      deleted_block = block_;
      topic = block_->topic;
      block_->topic = nullptr;
    }
  }

  if (deleted_block) {
    topic->Remove(deleted_block);
  }
}

}  // namespace cpppromise