#pragma once

#include "subscription.h"

namespace cpppromise {

template <typename T>
Subscription<T>::Subscription(
    std::shared_ptr<SubscriptionUnsubscribeTrigger<T>> trigger)
    : trigger_(trigger) {}

template <typename T>
void Subscription<T>::Unsubscribe() {
  trigger_->Unsubscribe();
}

}  // namespace cpppromise