#pragma once
#include <memory>
#include <string>

#include "event_queue_listener.h"
#include "promise_listener.h"
// Singleton Lifecycle Listener to generate lifecycle object for cpppromise
// stuff
class LifecycleListener {
 public:
  // id is to help filter interested and uninterested listener. If there is
  // uninterested listener, e.g. id is empty, we will return a nullptr to
  // indicate that the we are not interested in the lifecycle of the
  // corresponding thing

  // This method is called everytime we create a new Promise and the returned
  // listener will be plumbed into the newly created Promise object
  virtual std::shared_ptr<PromiseListener> OnPromiseCreated(std::string id) = 0;
  // This method is called everytime we create a new EventQueue and the returned
  // listener will be plumbed into the newly created EventQueue object
  virtual std::shared_ptr<EventQueueListener> OnEventQueueCreated(
      std::string id) = 0;
};
