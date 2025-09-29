#pragma once

#include "subscription_unsubscribe_trigger.h"

namespace cpppromise {

template <typename T>
class Publication;

template <typename T>
class Subscription {
 public:
  void Unsubscribe();

 private:
  friend class Publication<T>;

  explicit Subscription(
      std::shared_ptr<SubscriptionUnsubscribeTrigger<T>> trigger);

  std::shared_ptr<SubscriptionUnsubscribeTrigger<T>> trigger_;
};

}  // namespace cpppromise