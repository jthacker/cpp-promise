#pragma once

#include <cassert>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

#include "empty.h"
#include "event_queue_listener.h"
#include "timer.h"

namespace cpppromise {

template <typename T>
class Promise;

template <typename T>
class Resolver;

class Schedule;

class EventQueue {
 public:
  EventQueue(std::string id = "");

  ~EventQueue();

  void Join();

  void Finish();

  static EventQueue *Get();

  template <typename T>
  static std::pair<Promise<T>, Resolver<T>> CreateResolver(std::string id = "");

  template <typename T>
  static Promise<T> CreateResolvedPromise(T val, std::string id = "");

  template <typename T>
  Promise<T> Enqueue(std::function<T()> f, std::string id = "");

  Promise<Empty> Enqueue(std::function<void()> f, std::string id = "");

  template <typename T>
  Promise<T> EnqueueWithResolver(std::function<void(Resolver<T>)> resolve,
                                 std::string id = "");

  Schedule DoPeriodically(std::function<bool()> f,
                          std::chrono::nanoseconds interval,
                          std::string id = "");

  Schedule DoPeriodically(std::function<Promise<bool>()> f,
                          std::chrono::nanoseconds interval,
                          std::string id = "");

  struct Task {
    std::string id;
    std::shared_ptr<EventListener> e_listener;
    std::function<void()> f;
  };

 private:
  template <typename T>
  friend class PromiseControlBlock;
  friend class ScheduleControlBlock;

  void Start();
  void AddTask(const std::function<void()> &f, std::string id);
  void Take();
  void Release();

  std::thread t_;
  std::mutex mu_;
  std::mutex join_mu_;
  std::condition_variable cond_;
  std::deque<Task> tasks_;
  bool running_;
  int count_;
  std::shared_ptr<EventQueueListener> eq_listener_;
};

}  // namespace cpppromise
