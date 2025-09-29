#pragma once

#include <functional>
#include <string>

#include "subscription.h"

namespace cpppromise {

template <typename T>
class Topic;

template <typename T>
class Publication {
 public:
  Subscription<T> Subscribe(std::function<void(T)> listener,
                            std::string id = "");

 private:
  friend class Topic<T>;

  explicit Publication(Topic<T>* topic) : topic_(topic) {}

  Topic<T>* topic_;
};

}  // namespace cpppromise