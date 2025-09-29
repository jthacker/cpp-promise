#include "lifecycle_listener_manager.h"

#include <memory>

static std::shared_ptr<LifecycleListener> lifecycle_listener_ = nullptr;

void LifecycleListenerManager::Set(
    std::shared_ptr<LifecycleListener> listener) {
  lifecycle_listener_ = listener;
}

std::shared_ptr<LifecycleListener> LifecycleListenerManager::Get() {
  return lifecycle_listener_;
}