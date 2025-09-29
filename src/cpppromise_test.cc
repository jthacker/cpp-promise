#include "src/cpp_common/cpppromise/cpppromise.h"

#include <unordered_map>

#include "customized_test_listeners.h"
#include "gtest/gtest.h"
#include "src/cpp_common/cpppromise/non_csp_utils.h"

namespace {
constexpr int kLargeTestNumber = 1024;

class EventQueueTest : public ::testing::Test {
 public:
  void SetUp() override {
    q0_ = std::make_unique<cpppromise::EventQueue>();
    q1_ = std::make_unique<cpppromise::EventQueue>();
  }

  void Stop(std::unique_ptr<cpppromise::EventQueue>& q) {
    q->Finish();
    q->Join();
    q.reset(nullptr);
  }

  void Stop() {
    Stop(q0_);
    Stop(q1_);
  }

  std::unique_ptr<cpppromise::EventQueue> q0_;
  std::unique_ptr<cpppromise::EventQueue> q1_;
};

}  // namespace

namespace cpppromise {

TEST_F(EventQueueTest, BasicOperation) {
  int var = 0;
  q0_->Enqueue([&var]() { var = 1; });
  Stop();
  ASSERT_EQ(1, var);
}

TEST_F(EventQueueTest, PromiseCallbackNoInput) {
  bool done = false;
  q0_->Enqueue([&]() {
    Promise<int> p1 = q0_->Enqueue<int>([]() { return 1; });
    Promise<std::string> p2 = p1.Then<std::string>([]() { return "hello"; });
    p2.Then([&]() { done = true; });
  });

  Stop();
  ASSERT_TRUE(done);
}

TEST_F(EventQueueTest, OrderOfOperations) {
  std::vector<int> vec;
  q0_->Enqueue([&vec]() { vec.push_back(0); });
  q0_->Enqueue([&vec]() { vec.push_back(1); });
  Stop();
  ASSERT_EQ((std::vector<int>{0, 1}), vec);
}

TEST_F(EventQueueTest, Resolver) {
  int var = 0;
  q0_->Enqueue([this, &var]() {
    auto promise_resolver_pair = q0_->CreateResolver<int>();
    promise_resolver_pair.first.Then([&var](int k) { var = k; });
    q0_->Enqueue(
        [r = promise_resolver_pair.second]() mutable { r.Resolve(42); });
  });
  Stop();
  ASSERT_EQ(42, var);
}

TEST_F(EventQueueTest, CreateResolvedPromise) {
  int var = 0;
  q0_->Enqueue([this, &var]() {
    auto promise = q0_->CreateResolvedPromise(42);
    promise.Then([&var](int k) { var = k; });
  });
  Stop();
  ASSERT_EQ(42, var);
}

TEST_F(EventQueueTest, ResolveAll) {
  int expected_int = -1;
  double expected_double = -2;
  std::string expected_string = "";
  bool empty_resolved = false;

  q0_->Enqueue([&, this]() {
    Promise<int> p1 = q0_->Enqueue<int>([&]() {
      expected_int = 1;
      return expected_int;
    });

    Promise<double> p2 = q0_->Enqueue<double>([&]() {
      expected_double = 2.0;
      return expected_double;
    });
    Promise<std::string> p3 = q0_->Enqueue<std::string>([&]() {
      expected_string = "3";
      return expected_string;
    });
    Promise<Empty> p4 = q0_->Enqueue<Empty>([&]() {
      empty_resolved = true;
      return Empty();
    });
    Promise<Empty>::ResolveAll("", p1, p2, p3, p4).Then([&]() {
      ASSERT_EQ(expected_int, 1);
      ASSERT_EQ(expected_double, 2.0);
      ASSERT_EQ(expected_string, "3");
      ASSERT_TRUE(empty_resolved);
    });
  });

  Stop();
}

TEST_F(EventQueueTest, ResolverPromiseOrderOne) {
  const int max = 1024;
  std::vector<int> got;
  q0_->Enqueue([this, max, &got]() {
    for (int i = 0; i < max; i++) {
      q0_->Enqueue([&got, i]() { got.push_back(i); });
      auto promise_resolver_pair = q0_->CreateResolver<int>();
      promise_resolver_pair.first.Then([&got](int k) { got.push_back(k); });
      promise_resolver_pair.second.Resolve(2 * max + i);
    }
  });
  Stop();
  std::vector<int> want;
  for (int i = 0; i < max; i++) {
    want.push_back(i);
    want.push_back(2 * max + i);
  }
  ASSERT_EQ(got, want);
}

TEST_F(EventQueueTest, ResolverPromiseOrderTwo) {
  const int max = 1024;
  std::vector<int> got;
  q0_->Enqueue([this, max, &got]() {
    for (int i = 0; i < max; i++) {
      q0_->Enqueue([&got, i]() { got.push_back(i); });
      auto promise_resolver_pair = q0_->CreateResolver<int>();
      promise_resolver_pair.first.Then([&got](int k) { got.push_back(k); });
      q0_->Enqueue([r = promise_resolver_pair.second, max, i]() mutable {
        r.Resolve(2 * max + i);
      });
    }
  });
  Stop();
  std::vector<int> want;
  for (int i = 0; i < max; i++) {
    want.push_back(i);
  }
  for (int i = 0; i < max; i++) {
    want.push_back(2 * max + i);
  }
  ASSERT_EQ(got, want);
}

TEST_F(EventQueueTest, PromiseCopy) {
  q0_->Enqueue([this]() {
    cpppromise::Promise<int> p = q0_->Enqueue<int>([this]() { return 0; });
    for (int i = 0; i < 100; i++) {
      p = p.Then<int>([](int k) { return k + 1; });
    }
    p.Then([](int k) { ASSERT_EQ(100, k); });
  });
  Stop();
}

TEST_F(EventQueueTest, EnqueueWithResolver) {
  cpppromise::Promise<int> p = q0_->EnqueueWithResolver<int>(
      [](cpppromise::Resolver<int> resolver) { resolver.Resolve(42); });
  int result = 0;
  p.Then<cpppromise::Empty>(q0_.get(), [&](int v) {
    result = v;
    return cpppromise::Empty();
  });
  Stop();
  ASSERT_EQ(result, 42);
}

TEST_F(EventQueueTest, FlockOfPromises) {
  std::vector<int> results(kLargeTestNumber);
  for (int i = 0; i < kLargeTestNumber; i++) {
    results[i] = 0;
  }
  for (int i = 0; i < kLargeTestNumber; i++) {
    q0_->Enqueue<int>([i]() { return i; })
        .Then<cpppromise::Empty>(q0_.get(), [&results](int j) {
          results[j] = 1;
          return cpppromise::Empty();
        });
  }
  Stop();
  for (int i = 0; i < kLargeTestNumber; i++) {
    ASSERT_EQ(results[i], 1);
  }
}

TEST_F(EventQueueTest, StringOfPromises) {
  auto pr = cpppromise::EventQueue::CreateResolver<int>();
  cpppromise::Promise<int> p = pr.first;
  for (int i = 0; i < kLargeTestNumber; i++) {
    p = p.Then<int>(q0_.get(), [](int x) { return x + 1; });
  }
  int result = 0;
  p.Then<cpppromise::Empty>(q0_.get(), [&result](int i) {
    result = i;
    return cpppromise::Empty();
  });
  pr.second.Resolve(0);
  Stop();
  ASSERT_EQ(result, kLargeTestNumber);
}

TEST_F(EventQueueTest, FlockOfResolvedPromises) {
  const int arbitraryMultiplier = 42;
  const int numThreads = 100;
  const int itemsPerThread = kLargeTestNumber / numThreads;
  const int totalItems = itemsPerThread * numThreads;

  std::vector<cpppromise::Promise<int>> inputs;
  for (int i = 0; i < totalItems; i++) {
    auto pair = cpppromise::EventQueue::CreateResolver<int>();
    inputs.push_back(pair.first);
    pair.second.Resolve(i * arbitraryMultiplier);
  }
  cpppromise::Promise<int>* inputsData = inputs.data();

  std::vector<int> results(totalItems);
  std::vector<std::thread> threads;

  for (int i = 0; i < numThreads; i++) {
    threads.push_back(std::thread([this, i, inputsData, &results]() {
      for (int j = 0; j < itemsPerThread; j++) {
        const int index = itemsPerThread * i + j;
        inputsData[index].Then<cpppromise::Empty>(q0_.get(),
                                                  [index, &results](int v) {
                                                    results[index] = v;
                                                    return cpppromise::Empty();
                                                  });
        std::this_thread::sleep_for(std::chrono::microseconds(1));
      }
    }));
  }

  for (int i = 0; i < numThreads; i++) {
    threads[i].join();
  }

  Stop();

  for (int i = 0; i < totalItems; i++) {
    ASSERT_EQ(results[i], i * arbitraryMultiplier);
  }
}

TEST_F(EventQueueTest, ChattyEventQueues) {
  auto pr = cpppromise::EventQueue::CreateResolver<int>();
  cpppromise::Promise<int> p = pr.first;
  for (int i = 0; i < kLargeTestNumber; i++) {
    p = p.Then<int>(q0_.get(), [](int x) { return x + 1; });
    p = p.Then<int>(q1_.get(), [](int x) { return x + 1; });
  }
  int result = 0;
  p.Then<cpppromise::Empty>(q0_.get(), [&result](int i) {
    result = i;
    return cpppromise::Empty();
  });
  pr.second.Resolve(0);
  Stop();
  ASSERT_EQ(result, kLargeTestNumber * 2);
}

TEST_F(EventQueueTest, BouncingThen) {
  int result = 0;

  std::optional<cpppromise::Promise<int>> previous_promise;
  cpppromise::EventQueue* queue_this_time = q0_.get();

  for (int i = 0; i < kLargeTestNumber; i++) {
    if (!previous_promise.has_value()) {
      // Base case
      previous_promise = queue_this_time->EnqueueWithResolver<int>(
          [](cpppromise::Resolver<int> r) { r.Resolve(42); });
    } else {
      // Chain onto previous_promise
      cpppromise::Promise<int> p = *previous_promise;
      previous_promise = queue_this_time->EnqueueWithResolver<int>(
          [p](cpppromise::Resolver<int> r) mutable {
            p.Then([r](int k) mutable { r.Resolve(k + 1); });
          });
    }
    queue_this_time = (queue_this_time == q0_.get()) ? q1_.get() : q0_.get();
  }

  previous_promise->Then<cpppromise::Empty>(queue_this_time, [&result](int k) {
    result = k;
    return cpppromise::Empty();
  });
  Stop();

  ASSERT_EQ(result, 42 + kLargeTestNumber - 1);
}

class IntegerProducingProcess : public cpppromise::Process {
 public:
  void Join() override { cpppromise::Process::Join(); }

  cpppromise::Promise<int> GetNext(cpppromise::Promise<int> previous) {
    return EnqueueWithResolver<int>(
        [previous](cpppromise::Resolver<int> r) mutable {
          previous.Then(std::function<void(int)>(
              [r](int k) mutable { r.Resolve(k + 1); }));
        });
  }
  void Done() { Finish(); }
};

class TestProcess : public cpppromise::Process {
 public:
  TestProcess(bool wait) : final_result_(0) {
    Enqueue([this]() { Start(); });
  }

  void Join() override {
    ipp.Join();
    cpppromise::Process::Join();
  }

  cpppromise::Promise<int> GetNext(cpppromise::Promise<int> previous) {
    return EnqueueWithResolver<int>(
        [previous](cpppromise::Resolver<int> r) mutable {
          previous.Then(std::function<void(int)>(
              [r](int k) mutable { r.Resolve(k + 1); }));
        });
  }

  void Start() {
    std::pair<cpppromise::Promise<int>, cpppromise::Resolver<int>>
        promise_resolver_pair = CreateResolver<int>();
    auto p = promise_resolver_pair.first;

    for (int i = 0; i < 100; i++) {
      p = ipp.GetNext(p);
    }

    p.Then([this](int k) { final_result_ = k; });

    promise_resolver_pair.second.Resolve(0);

    Enqueue([this]() {
      ipp.Done();
      Finish();
    });
  }

  int final_result_;
  IntegerProducingProcess ipp;
};

TEST(CppPromiseTest, TestProcess) {
  TestProcess test(false);
  test.Join();
  ASSERT_EQ(test.final_result_, 100);
}

TEST(DoPeriodicallyTest, TestPeriodicExecution) {
  const std::chrono::nanoseconds delta_t = std::chrono::nanoseconds(5000000);
  const int iteration_count = 100;
  int count = 0;

  auto start = std::chrono::steady_clock::now();

  cpppromise::EventQueue q;

  cpppromise::Get<cpppromise::Empty>(q.DoPeriodically(
                                          [&]() {
                                            if (++count == iteration_count) {
                                              return false;
                                            }
                                            return true;
                                          },
                                          delta_t)
                                         .Done());

  std::chrono::nanoseconds elapsed = std::chrono::steady_clock::now() - start;
  // Accept 10% "jitter" error in the rate
  EXPECT_NEAR(elapsed.count(), delta_t.count() * iteration_count,
              0.1 * delta_t.count() * iteration_count);

  EXPECT_EQ(count, iteration_count);

  q.Finish();
  q.Join();
}

TEST(DoPeriodicallyTest, TestZeroInterval) {
  const int iteration_count = 100;
  int count = 0;

  cpppromise::EventQueue q;

  cpppromise::Get<cpppromise::Empty>(q.DoPeriodically(
                                          [&]() {
                                            if (++count == iteration_count) {
                                              return false;
                                            }
                                            return true;
                                          },
                                          std::chrono::nanoseconds(0))
                                         .Done());

  EXPECT_EQ(count, iteration_count);

  q.Finish();
  q.Join();
}

TEST(DoPeriodicallyTest, CanResolveIfEventQueueIsDestroyed) {
  const std::chrono::nanoseconds delta_t = std::chrono::nanoseconds(5000);

  cpppromise::EventQueue q;

  {
    cpppromise::Promise<cpppromise::Empty> p =
        q.DoPeriodically([&]() { return true; }, delta_t).Done();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    q.Finish();
    cpppromise::Get(p);
  }

  q.Join();
}

TEST(DoPeriodicallyTest, NoopIfEventQueueIsDestroyed) {
  const std::chrono::nanoseconds delta_t = std::chrono::nanoseconds(5000);
  int count = 0;

  cpppromise::EventQueue q;

  {
    cpppromise::Promise<cpppromise::Empty> p = q.DoPeriodically(
                                                    [&]() {
                                                      count++;
                                                      return true;
                                                    },
                                                    delta_t)
                                                   .Done();

    q.Finish();
    cpppromise::Get(p);
    EXPECT_EQ(count, 0);
  }

  q.Join();
}

TEST(DoPeriodicallyTest, TestPeriodicExecutionReturnsPromise) {
  const std::chrono::nanoseconds delta_t = std::chrono::nanoseconds(5000000);
  const int iteration_count = 100;
  int count = 0;

  auto start = std::chrono::steady_clock::now();

  cpppromise::EventQueue q1;
  cpppromise::EventQueue q2;

  cpppromise::Get<cpppromise::Empty>(q1.DoPeriodically(
                                           [&]() {
                                             return q2.Enqueue<bool>([&]() {
                                               if (++count == iteration_count) {
                                                 return false;
                                               }
                                               return true;
                                             });
                                           },
                                           delta_t)
                                         .Done());

  std::chrono::nanoseconds elapsed = std::chrono::steady_clock::now() - start;
  // Accept 10% "jitter" error in the rate
  EXPECT_NEAR(elapsed.count(), delta_t.count() * iteration_count,
              0.1 * delta_t.count() * iteration_count);

  EXPECT_EQ(count, iteration_count);

  q1.Finish();
  q2.Finish();
  q1.Join();
  q2.Join();
}

TEST(DoPeriodicallyTest, TestCancel) {
  const std::chrono::nanoseconds delta_t = std::chrono::nanoseconds(1);
  cpppromise::EventQueue q;
  std::condition_variable v;
  std::mutex mu;
  bool called = false;

  {
    auto schedule = q.DoPeriodically(
        [&]() {
          std::unique_lock<std::mutex> lock(mu);
          called = true;
          v.notify_one();
          return true;
        },
        delta_t);

    {
      std::unique_lock<std::mutex> lock(mu);
      v.wait(lock, [&]() { return !called; });
    }

    schedule.Cancel();

    cpppromise::Get<cpppromise::Empty>(schedule.Done());
  }

  q.Finish();
  q.Join();
}

TEST(LifecycleTest, LifecycleCreated) {
  std::shared_ptr<LifecycleListener> listener =
      std::make_shared<CustomizedLifecycleListener>();
  LifecycleListenerManager::Set(listener);

  std::shared_ptr<LifecycleListener> retrieved_listener =
      LifecycleListenerManager::Get();

  EXPECT_NE(retrieved_listener->OnEventQueueCreated("ListenerTest"), nullptr);

  EXPECT_EQ(retrieved_listener->OnEventQueueCreated("different string"),
            nullptr);

  EXPECT_NE(retrieved_listener->OnPromiseCreated("ListenerTest"), nullptr);

  EXPECT_EQ(retrieved_listener->OnPromiseCreated("different string"), nullptr);
}

TEST(LifecycleTest, LatencyTest) {
  std::shared_ptr<CustomizedLifecycleListener> listener =
      std::make_shared<CustomizedLifecycleListener>();
  LifecycleListenerManager::Set(listener);
  auto q0_ = std::make_unique<cpppromise::EventQueue>("ListenerTest");
  q0_->Enqueue(
      []() { std::this_thread::sleep_for(std::chrono::milliseconds(100)); },
      "ListenerTest");
  q0_->Finish();
  q0_->Join();
  q0_.reset(nullptr);
  EXPECT_NEAR(listener->GetEventQueueListenerMap()["ListenerTest"]
                  ->GetEventListenerMap()["ListenerTest"]
                  ->GetLatency(),
              100, 10);
  EXPECT_NEAR(listener->GetPromiseListenerMap()["ListenerTest"]->GetLatency(),
              100, 10);
}

}  // namespace cpppromise