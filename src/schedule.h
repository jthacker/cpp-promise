#pragma once

#include "empty.h"
#include "promise.h"
#include "schedule_cancel_trigger.h"

namespace cpppromise {

class Schedule {
 public:
  Promise<Empty> Done();

  void Cancel();

 private:
  friend class EventQueue;

  Schedule(std::shared_ptr<ScheduleCancelTrigger> trigger, Promise<Empty> done);

  std::shared_ptr<ScheduleCancelTrigger> trigger_;
  Promise<Empty> done_;
};

}  // namespace cpppromise