#pragma once

#include <memory>

#include "lifecycle_listener.h"
class LifecycleListenerManager {
 public:
  static void Set(std::shared_ptr<LifecycleListener> listener);

  static std::shared_ptr<LifecycleListener> Get();
};