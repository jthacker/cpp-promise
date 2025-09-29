#include "schedule_cancel_trigger.h"

#include <memory>

namespace cpppromise {

ScheduleCancelTrigger::ScheduleCancelTrigger(
    std::shared_ptr<ScheduleControlBlock> block)
    : block_(block) {}

ScheduleCancelTrigger::~ScheduleCancelTrigger() { block_->Cancel(); }

void ScheduleCancelTrigger::Cancel() { block_->Cancel(); }

}  // namespace cpppromise