#pragma once

// abstract class for which user needs to provide concrete implementation for
// interested lifecycle event of a single event
class EventListener {
 public:
  virtual void OnEnqueued() = 0;
  virtual void OnDequeued() = 0;
  virtual void OnStarted() = 0;
  virtual void OnCompleted() = 0;
};