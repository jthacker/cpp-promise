#include "event_queue.h"

#include "event_queue_impl.h"
#include "lifecycle_listener_manager.h"
#include "promise.h"
#include "promise_control_block.h"
#include "promise_control_block_impl.h"
#include "promise_impl.h"
#include "resolver.h"
#include "resolver_impl.h"
#include "schedule.h"
#include "schedule_cancel_trigger.h"
#include "schedule_control_block.h"
#include "timer.h"

namespace cpppromise {

// A thread's EventQueue is held as a thread-local value, so that clients do not
// always have to supply it with every function call.
inline thread_local EventQueue *__thread_q__ = nullptr;

EventQueue::EventQueue(std::string id) : running_(true), count_(0) {
  if (LifecycleListenerManager::Get()) {
    eq_listener_ = LifecycleListenerManager::Get()->OnEventQueueCreated(id);
  }
  Start();
}

EventQueue *EventQueue::Get() { return __thread_q__; }

EventQueue::~EventQueue() {
  assert(Get() != this);
  Finish();
  Join();
}

void EventQueue::AddTask(const std::function<void()> &f, std::string id) {
  std::unique_lock<std::mutex> lock(mu_);
  std::shared_ptr<EventListener> e_listener;
  if (eq_listener_) {
    e_listener = eq_listener_->OnEventEnqueued(id);
  }
  if (e_listener) {
    e_listener->OnEnqueued();
  }
  Task task = {id, e_listener, f};
  tasks_.push_back(task);
  cond_.notify_one();
}

void EventQueue::Start() {
  t_ = std::thread([this]() {
    __thread_q__ = this;
    while (true) {
      Task task;
      {
        std::unique_lock<std::mutex> lock(mu_);
        if (tasks_.empty()) {
          if (!running_ && count_ == 0) {
            break;
          } else {
            cond_.wait(lock);
            continue;
          }
        }
        task = tasks_.front();
        tasks_.pop_front();
        if (task.e_listener) {
          task.e_listener->OnDequeued();
        }
        if (eq_listener_) {
          eq_listener_->OnEventDequeued(task.id);
        }
      }
      if (task.e_listener) {
        task.e_listener->OnStarted();
      }
      task.f();
      if (task.e_listener) {
        task.e_listener->OnCompleted();
      }
    }
    __thread_q__ = nullptr;
  });
}

void EventQueue::Join() {
  std::unique_lock<std::mutex> lock(join_mu_);
  if (t_.joinable()) {
    t_.join();
  }
}

void EventQueue::Finish() {
  std::unique_lock<std::mutex> lock(mu_);
  running_ = false;
  cond_.notify_one();
}

Promise<Empty> EventQueue::Enqueue(std::function<void(void)> f,
                                   std::string id) {
  return Enqueue<Empty>(
      [f]() {
        f();
        return Empty{};
      },
      id);
}

void EventQueue::Take() {
  std::unique_lock<std::mutex> lock(mu_);
  count_++;
}

void EventQueue::Release() {
  std::unique_lock<std::mutex> lock(mu_);
  count_--;
  cond_.notify_one();
}

Schedule EventQueue::DoPeriodically(std::function<Promise<bool>()> f,
                                    std::chrono::nanoseconds interval,
                                    std::string id) {
  std::pair<Promise<Empty>, Resolver<Empty>> done_pair =
      CreateResolver<Empty>();
  std::shared_ptr<ScheduleControlBlock> scb =
      std::make_shared<ScheduleControlBlock>(this, f, interval, id,
                                             done_pair.second);
  std::shared_ptr<ScheduleCancelTrigger> sct =
      std::make_shared<ScheduleCancelTrigger>(scb);
  scb->Start();
  return Schedule(sct, done_pair.first);
}

Schedule EventQueue::DoPeriodically(std::function<bool()> f,
                                    std::chrono::nanoseconds interval,
                                    std::string id) {
  return DoPeriodically(
      [f]() {
        auto pair = CreateResolver<bool>();
        pair.second.Resolve(f());
        return pair.first;
      },
      interval, id);
}

}  // namespace cpppromise
