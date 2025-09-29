#include "timer.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace cpppromise {
namespace {

TEST(TimerTest, SortedIntervals) {
  constexpr int n = 50;
  std::mutex mu;
  std::unordered_map<int, Timer::clock::time_point> times;
  std::condition_variable cond;

  auto testInterval = [](int i) { return std::chrono::milliseconds(i * 5); };

  for (int i = n - 1; i >= 0; i--) {
    Timer::Get()->Schedule(Timer::Get()->Now() + testInterval(i),
                           [i, &times, &mu, &cond]() {
                             std::unique_lock<std::mutex> lock(mu);
                             times.insert({i, Timer::Get()->Now()});
                             if (times.size() == n) {
                               cond.notify_one();
                             }
                           });
  }

  Timer::clock::time_point now = Timer::Get()->Now();

  {
    std::unique_lock<std::mutex> lock(mu);
    cond.wait(lock, [&times]() { return times.size() == n; });
  }

  // Testing for timing accuracy in a general purpose unit test is inherently
  // flakey, so we choose a large "n" of jobs and compute the average accuracy
  // of timing.

  long deltaMicrosTotal = 0;

  for (int i = 0; i < n; i++) {
    auto delta = times[i] - testInterval(i) - now;
    deltaMicrosTotal += std::abs(
        std::chrono::duration_cast<std::chrono::microseconds>(delta).count());
  }

  EXPECT_LT(deltaMicrosTotal / n, 1000);
}

TEST(TimerTest, InThePast) {
  std::mutex mu;
  std::condition_variable cond;
  bool called = false;

  Timer::Get()->Schedule(Timer::Get()->Now() - std::chrono::milliseconds(1000),
                         [&mu, &cond, &called]() {
                           std::unique_lock<std::mutex> lock(mu);
                           called = true;
                           cond.notify_one();
                         });

  {
    std::unique_lock<std::mutex> lock(mu);
    cond.wait(lock, [&called]() { return called; });
  }
}

TEST(TimerTest, Now) {
  std::mutex mu;
  std::condition_variable cond;
  bool called = false;

  Timer::Get()->Schedule(Timer::Get()->Now(), [&mu, &cond, &called]() {
    std::unique_lock<std::mutex> lock(mu);
    called = true;
    cond.notify_one();
  });

  {
    std::unique_lock<std::mutex> lock(mu);
    cond.wait(lock, [&called]() { return called; });
  }
}

TEST(TimerTest, NotCalledTwice) {
  const std::chrono::milliseconds interval(1);
  std::mutex mu;
  std::condition_variable cond;
  int num_calls = 0;

  Timer::Get()->Schedule(Timer::Get()->Now() + interval,
                         [&mu, &cond, &num_calls]() {
                           std::unique_lock<std::mutex> lock(mu);
                           num_calls++;
                           cond.notify_one();
                         });

  {
    std::unique_lock<std::mutex> lock(mu);
    cond.wait(lock, [&num_calls]() { return num_calls != 0; });
    EXPECT_EQ(num_calls, 1);
  }

  std::this_thread::sleep_for(interval * 10);

  {
    std::unique_lock<std::mutex> lock(mu);
    EXPECT_EQ(num_calls, 1);
  }
}

TEST(TimerTest, CanCancel) {
  const std::chrono::milliseconds interval(1);
  std::mutex mu;
  bool called0 = false;
  bool called1 = false;

  uint64_t id0 =
      Timer::Get()->Schedule(Timer::Get()->Now() + interval, [&mu, &called0]() {
        std::unique_lock<std::mutex> lock(mu);
        called0 = true;
      });

  Timer::Get()->Schedule(Timer::Get()->Now() + interval, [&mu, &called1]() {
    std::unique_lock<std::mutex> lock(mu);
    called1 = true;
  });

  {
    std::unique_lock<std::mutex> lock(mu);
    EXPECT_FALSE(called0);
    EXPECT_FALSE(called1);
    EXPECT_EQ(Timer::Get()->Cancel(id0), true);
  }

  std::this_thread::sleep_for(interval * 10);

  {
    std::unique_lock<std::mutex> lock(mu);
    EXPECT_FALSE(called0);
    EXPECT_TRUE(called1);
  }
}

}  // namespace
}  // namespace cpppromise
