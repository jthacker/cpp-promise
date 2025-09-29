#pragma once

#include <functional>
#include <memory>

#include "empty.h"
#include "promise.h"
#include "resolver.h"

namespace cpppromise {

template <typename T>
class Publication;

template <typename T>
class SubscriptionUnsubscribeTrigger;

template <typename T>
struct SubscriptionControlBlock;

template <typename T>
class Topic {
 public:
  Topic() : publication_(this) {}

  Publication<T>& GetPublication() { return publication_; }

  Promise<Empty> Publish(T value);

 private:
  friend class Publication<T>;
  friend class SubscriptionUnsubscribeTrigger<T>;

  void Add(std::shared_ptr<SubscriptionControlBlock<T>> block);
  void Remove(std::shared_ptr<SubscriptionControlBlock<T>> block);

  static void ResolvePublishPromiseWhenRecipientsDone(
      Resolver<cpppromise::Empty> resolver,
      std::vector<Promise<Empty>> promises);

  std::mutex mu_;
  Publication<T> publication_;
  std::vector<std::shared_ptr<SubscriptionControlBlock<T>>> subscriptions_;
};

}  // namespace cpppromise