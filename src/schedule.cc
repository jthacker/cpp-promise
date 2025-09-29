#include "schedule.h"

namespace cpppromise {

Schedule::Schedule(std::shared_ptr<ScheduleCancelTrigger> trigger,
                   Promise<Empty> done)
    : trigger_(trigger), done_(done) {}

Promise<Empty> Schedule::Done() { return done_; }

void Schedule::Cancel() { trigger_->Cancel(); }

}  // namespace cpppromise