#include "process.h"

namespace cpppromise {

Process::Process(std::string id) : q_(id) {}

Promise<Empty> Process::Enqueue(std::function<void(void)> f, std::string id) {
  return q_.Enqueue(f, id);
}

Schedule Process::DoPeriodically(std::function<bool()> f,
                                 std::chrono::nanoseconds interval,
                                 std::string id) {
  return q_.DoPeriodically(f, interval, id);
};

Schedule Process::DoPeriodically(std::function<Promise<bool>()> f,
                                 std::chrono::nanoseconds interval,
                                 std::string id) {
  return q_.DoPeriodically(f, interval, id);
}

}  // namespace cpppromise