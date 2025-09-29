#pragma once

#include "publication.h"
#include "subscription_control_block.h"
#include "topic.h"

namespace cpppromise {

template <typename T>
Promise<Empty> Topic<T>::Publish(T value) {
  std::unique_lock<std::mutex> lock(mu_);
  std::vector<Promise<Empty>> completions;
  for (const auto &i : subscriptions_) {
    std::shared_ptr<SubscriptionControlBlock<T>> block = i;
    completions.push_back(block->q->Enqueue(
        [value, block]() {
          {
            std::unique_lock<std::mutex> lock(block->mu);
            if (block->topic == nullptr) {
              return;
            }
          }
          block->listener(value);
        },
        block->id));
  }
  auto promise_resolver_pair = EventQueue::Get()->CreateResolver<Empty>();
  ResolvePublishPromiseWhenRecipientsDone(promise_resolver_pair.second,
                                          completions);
  return promise_resolver_pair.first;
}

template <typename T>
void Topic<T>::Add(std::shared_ptr<SubscriptionControlBlock<T>> block) {
  std::unique_lock<std::mutex> lock(mu_);
  subscriptions_.push_back(block);
}

template <typename T>
void Topic<T>::Remove(std::shared_ptr<SubscriptionControlBlock<T>> block) {
  std::unique_lock<std::mutex> lock(mu_);
  for (auto i = subscriptions_.begin(); i != subscriptions_.end(); i++) {
    if (*i == block) {
      subscriptions_.erase(i);
      break;
    }
  }
}

template <typename T>
void Topic<T>::ResolvePublishPromiseWhenRecipientsDone(
    Resolver<cpppromise::Empty> resolver,
    std::vector<Promise<Empty>> promises) {
  EventQueue::Get()->Enqueue([resolver, promises]() mutable {
    if (promises.empty()) {
      resolver.Resolve(Empty());
    } else {
      Promise<Empty> next_promise = *promises.begin();
      promises.erase(promises.begin());
      next_promise.Then([resolver, promises]() {
        Topic::ResolvePublishPromiseWhenRecipientsDone(resolver, promises);
      });
    }
  });
}

}  // namespace cpppromise