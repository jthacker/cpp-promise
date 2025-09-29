#pragma once

#include <memory>

#include "schedule_control_block.h"

namespace cpppromise {

class ScheduleCancelTrigger {
 public:
  explicit ScheduleCancelTrigger(std::shared_ptr<ScheduleControlBlock> block);
  ~ScheduleCancelTrigger();

  void Cancel();

 private:
  std::shared_ptr<ScheduleControlBlock> block_;
};

}  // namespace cpppromise