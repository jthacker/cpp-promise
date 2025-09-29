#pragma once
#include <memory>
#include <string>

#include "event_listener.h"
// abstract class for which user needs to provide concrete implementation for
// interested lifecycle event of event queue.
class EventQueueListener {
 public:
  virtual std::shared_ptr<EventListener> OnEventEnqueued(std::string id) = 0;
  virtual void OnEventDequeued(std::string id) = 0;
};
