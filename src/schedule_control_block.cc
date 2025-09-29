#include "schedule_control_block.h"

#include "promise_control_block.h"
#include "promise_control_block_impl.h"
#include "promise_impl.h"

namespace cpppromise {

ScheduleControlBlock::ScheduleControlBlock(EventQueue* q,
                                           std::function<Promise<bool>()> f,
                                           std::chrono::nanoseconds interval,
                                           std::string id, Resolver<Empty> done)
    : q_(q),
      f_(f),
      interval_(interval),
      id_(id),
      running_(true),
      scheduled_run_time_(),
      done_(done) {
  q_->Take();
}

ScheduleControlBlock::~ScheduleControlBlock() {
  std::unique_lock<std::mutex> lock(mu_);
  assert(!running_);
  q_->Release();
}

void ScheduleControlBlock::Start() { ScheduleNextRun(); }

void ScheduleControlBlock::Cancel() {
  {
    std::unique_lock<std::mutex> lock(mu_);
    if (current_timer_.has_value()) {
      Timer::Get()->Cancel(*current_timer_);
      current_timer_.reset();
    }
  }

  Finish();
}

void ScheduleControlBlock::Finish() {
  std::unique_lock<std::mutex> lock(mu_);
  if (running_) {
    running_ = false;
    done_.Resolve(Empty());
  }
}

void ScheduleControlBlock::TimerCallback() {
  std::unique_lock<std::mutex> lock(mu_);
  current_timer_.reset();
  q_->Enqueue(
      [shared_this = shared_from_this()]() {
        std::unique_lock<std::mutex> lock(shared_this->mu_);
        if (!shared_this->running_) {
          return;
        }
        shared_this->f_().Then([shared_this](bool keep_running) {
          if (!keep_running) {
            shared_this->Finish();
          } else {
            shared_this->ScheduleNextRun();
          }
        });
      },
      id_);
}

void ScheduleControlBlock::ScheduleNextRun() {
  std::unique_lock<std::mutex> lock(mu_);

  if (!running_) {
    return;
  }

  if (scheduled_run_time_ == Timer::clock::time_point()) {
    scheduled_run_time_ = Timer::Get()->Now();
  } else {
    scheduled_run_time_ = scheduled_run_time_ + interval_;
  }

  current_timer_ = Timer::Get()->Schedule(
      scheduled_run_time_,
      [shared_this = shared_from_this()]() { shared_this->TimerCallback(); });
}

}  // namespace cpppromise