#pragma once
#include <chrono>
#include <iostream>
#include <unordered_map>

#include "cpppromise.h"

class CustomizedPromiseListener : public PromiseListener {
 public:
  CustomizedPromiseListener() {
    add_time_ = std::chrono::high_resolution_clock::now();
  }
  void OnResolved() override {
    std::cout << "Promise Resolved" << std::endl;
    end_time_ = std::chrono::high_resolution_clock::now();
  }
  long GetLatency() {
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                       end_time_ - add_time_)
                       .count();
    return latency;
  }

 private:
  std::chrono::high_resolution_clock::time_point add_time_;
  std::chrono::high_resolution_clock::time_point end_time_;
};

class CustomizedEventListener : public EventListener {
 public:
  void OnEnqueued() override { std::cout << "Event Enqueued" << std::endl; }
  void OnDequeued() override { std::cout << "Event Dequeued" << std::endl; }
  void OnStarted() override {
    std::cout << "Event Started" << std::endl;
    add_time_ = std::chrono::high_resolution_clock::now();
  }
  void OnCompleted() override {
    std::cout << "Event Completed" << std::endl;
    end_time_ = std::chrono::high_resolution_clock::now();
  }
  long GetLatency() {
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                       end_time_ - add_time_)
                       .count();
    return latency;
  }

 private:
  std::chrono::high_resolution_clock::time_point add_time_;
  std::chrono::high_resolution_clock::time_point end_time_;
};

class CustomizedEventQueueListener : public EventQueueListener {
 public:
  std::shared_ptr<EventListener> OnEventEnqueued(std::string id) override {
    if (id == "ListenerTest") {
      event_listener_map_[id] = std::make_shared<CustomizedEventListener>();
      return event_listener_map_[id];
    }
    return nullptr;
  }
  void OnEventDequeued(std::string id) override { return; }
  std::unordered_map<std::string, std::shared_ptr<CustomizedEventListener>>
  GetEventListenerMap() {
    return event_listener_map_;
  }

 private:
  std::unordered_map<std::string, std::shared_ptr<CustomizedEventListener>>
      event_listener_map_;
};

class CustomizedLifecycleListener : public LifecycleListener {
 public:
  std::shared_ptr<EventQueueListener> OnEventQueueCreated(
      std::string id) override {
    if (id == "ListenerTest") {
      event_queue_listener_map_[id] =
          std::make_shared<CustomizedEventQueueListener>();
      return event_queue_listener_map_[id];
    }
    return nullptr;
  }
  std::shared_ptr<PromiseListener> OnPromiseCreated(std::string id) override {
    if (id == "ListenerTest") {
      promise_listener_map_[id] = std::make_shared<CustomizedPromiseListener>();
      return promise_listener_map_[id];
    }
    return nullptr;
  }

  std::unordered_map<std::string, std::shared_ptr<CustomizedEventQueueListener>>
  GetEventQueueListenerMap() {
    return event_queue_listener_map_;
  }

  std::unordered_map<std::string, std::shared_ptr<CustomizedPromiseListener>>
  GetPromiseListenerMap() {
    return promise_listener_map_;
  }

 private:
  std::unordered_map<std::string, std::shared_ptr<CustomizedEventQueueListener>>
      event_queue_listener_map_;
  std::unordered_map<std::string, std::shared_ptr<CustomizedPromiseListener>>
      promise_listener_map_;
};