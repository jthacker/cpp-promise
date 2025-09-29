#pragma once

#include <memory>

#include "subscription_control_block.h"

namespace cpppromise {

template <typename T>
class SubscriptionUnsubscribeTrigger {
 public:
  explicit SubscriptionUnsubscribeTrigger(
      std::shared_ptr<SubscriptionControlBlock<T>> block);
  ~SubscriptionUnsubscribeTrigger();

  void Unsubscribe();

 private:
  std::shared_ptr<SubscriptionControlBlock<T>> block_;
};

}  // namespace cpppromise