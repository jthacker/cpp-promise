#include "src/cpp_common/cpppromise/cpppromise_stream.h"

#include <condition_variable>
#include <mutex>

#include "customized_test_listeners.h"
#include "gtest/gtest.h"
#include "src/cpp_common/cpppromise/non_csp_utils.h"

class PublisherProcess : public cpppromise::Process {
 public:
  explicit PublisherProcess() : count_(0) {}

  void Join() {
    Finish();
    cpppromise::Process::Join();
  }

  cpppromise::Publication<int>& GetNumbers() {
    return numbers_.GetPublication();
  }

  cpppromise::Promise<cpppromise::Empty> Publish() {
    return Enqueue([this]() { numbers_.Publish(count_++); });
  }

  cpppromise::Topic<int> numbers_;
  int count_;
};

class ConsumerProcess : public cpppromise::Process {
 public:
  ConsumerProcess(PublisherProcess* pp, std::string id = "")
      : cpppromise::Process(id), pp_(pp) {}

  void Join() {
    Finish();
    cpppromise::Process::Join();
  }

  cpppromise::Promise<cpppromise::Empty> StartConsuming(
      std::string callback_event_id = "") {
    return Enqueue([this, callback_event_id]() {
      subscription_ = pp_->GetNumbers().Subscribe(
          [this](int k) { received_.push_back(k); }, callback_event_id);
    });
  }

  cpppromise::Promise<cpppromise::Empty> StopConsuming() {
    return Enqueue([this]() {
      subscription_->Unsubscribe();
      subscription_.reset();
    });
  }

  PublisherProcess* pp_;
  std::vector<int> received_;
  std::optional<cpppromise::Subscription<int>> subscription_;
};

TEST(CppPromiseStreamTest, NoSubscriptionCase) {
  PublisherProcess publisher;
  ConsumerProcess consumer(&publisher);

  for (int i = 0; i < 10; i++) {
    publisher.Publish();
  }

  publisher.Join();
  consumer.Join();

  ASSERT_EQ(consumer.received_.size(), 0);
}

TEST(CppPromiseStreamTest, SimpleSubscription) {
  PublisherProcess publisher;
  ConsumerProcess consumer(&publisher);
  cpppromise::EventQueue q;

  std::mutex mu;
  std::condition_variable cond;
  bool done = false;

  consumer.StartConsuming().Then<cpppromise::Empty>(&q, [&](cpppromise::Empty) {
    publisher.Publish().Then([&]() {
      consumer.StopConsuming();
      q.Enqueue([&]() {
        std::unique_lock<std::mutex> lock(mu);
        done = true;
        cond.notify_one();
      });
      return cpppromise::Empty();
    });
    return cpppromise::Empty();
  });

  std::unique_lock<std::mutex> lock(mu);
  cond.wait(lock, [&]() { return done; });

  publisher.Join();
  consumer.Join();

  ASSERT_EQ(consumer.received_.size(), 1);
  ASSERT_EQ(consumer.received_[0], 0);
}

TEST(CppPromiseStreamTest, LongerSubscription) {
  PublisherProcess publisher;
  ConsumerProcess consumer(&publisher);
  cpppromise::EventQueue q;

  std::mutex mu;
  std::condition_variable cond;
  bool done = false;

  consumer.StartConsuming().Then<cpppromise::Empty>(&q, [&](cpppromise::Empty) {
    for (int i = 0; i < 10; i++) {
      publisher.Publish();
    }

    publisher.Publish().Then<cpppromise::Empty>(&q, [&](cpppromise::Empty) {
      consumer.StopConsuming();
      std::unique_lock<std::mutex> lock(mu);
      done = true;
      cond.notify_one();
      return cpppromise::Empty();
    });

    return cpppromise::Empty();
  });

  std::unique_lock<std::mutex> lock(mu);
  cond.wait(lock, [&]() { return done; });

  publisher.Join();
  consumer.Join();

  ASSERT_EQ(consumer.received_.size(), 11);
  ASSERT_EQ(consumer.received_[0], 0);
  ASSERT_EQ(consumer.received_[10], 10);
}

TEST(CppPromiseStreamTest, LatencyTest) {
  std::shared_ptr<CustomizedLifecycleListener> listener =
      std::make_shared<CustomizedLifecycleListener>();
  LifecycleListenerManager::Set(listener);

  PublisherProcess publisher;
  ConsumerProcess consumer(&publisher, "ListenerTest");
  cpppromise::EventQueue q;

  std::mutex mu;
  std::condition_variable cond;
  bool done = false;

  consumer.StartConsuming("ListenerTest")
      .Then<cpppromise::Empty>(&q, [&](cpppromise::Empty) {
        publisher.Publish().Then([&]() {
          consumer.StopConsuming();
          q.Enqueue([&]() {
            std::unique_lock<std::mutex> lock(mu);
            done = true;
            cond.notify_one();
          });
          return cpppromise::Empty();
        });
        return cpppromise::Empty();
      });

  std::unique_lock<std::mutex> lock(mu);
  cond.wait(lock, [&]() { return done; });

  publisher.Join();
  consumer.Join();

  ASSERT_EQ(consumer.received_.size(), 1);
  ASSERT_EQ(consumer.received_[0], 0);

  EXPECT_GE(listener->GetEventQueueListenerMap()["ListenerTest"]
                ->GetEventListenerMap()["ListenerTest"]
                ->GetLatency(),
            0);
  EXPECT_GE(listener->GetPromiseListenerMap()["ListenerTest"]->GetLatency(), 0);
}

class OutOfScopingConsumerProcess : public cpppromise::Process {
 public:
  OutOfScopingConsumerProcess(PublisherProcess* pp, int bound)
      : cpppromise::Process(), pp_(pp), bound_(bound) {}

  cpppromise::Promise<cpppromise::Empty> StartConsuming() {
    return Enqueue([this]() {
      subscription_ = pp_->GetNumbers().Subscribe([this](int k) {
        if (k == bound_) {
          subscription_.reset();
          return;
        }
        received_.push_back(k);
      });
    });
  }

  void Join() {
    Finish();
    cpppromise::Process::Join();
  }

  PublisherProcess* pp_;
  std::vector<int> received_;
  std::optional<cpppromise::Subscription<int>> subscription_;
  int bound_;
};

TEST(CppPromiseStreamTest, SubscriptionOutOfScopeUnsubscribes) {
  const int bound = 512;
  const int large_message_count = 4096;

  PublisherProcess publisher;
  OutOfScopingConsumerProcess consumer(&publisher, bound);
  cpppromise::Get(consumer.StartConsuming());

  for (int i = 0; i < large_message_count; i++) {
    cpppromise::Get(publisher.Publish());
  }

  publisher.Join();
  consumer.Join();

  ASSERT_EQ(consumer.received_.size(), bound);
}

class BoundedConsumerProcess : public cpppromise::Process {
 public:
  BoundedConsumerProcess(PublisherProcess* pp, int bound)
      : cpppromise::Process(), pp_(pp), bound_(bound) {}

  cpppromise::Promise<cpppromise::Empty> StartConsuming() {
    return Enqueue([this]() {
      subscription_ = pp_->GetNumbers().Subscribe([this](int k) {
        if (k == bound_) {
          subscription_->Unsubscribe();
          return;
        }
        received_.push_back(k);
      });
    });
  }

  void Join() {
    Finish();
    cpppromise::Process::Join();
  }

  PublisherProcess* pp_;
  std::vector<int> received_;
  std::optional<cpppromise::Subscription<int>> subscription_;
  int bound_;
};

TEST(CppPromiseStreamTest, StreamUnsubscribeIsImmediate) {
  const int bound = 512;
  const int large_message_count = 4096;

  PublisherProcess publisher;
  BoundedConsumerProcess consumer(&publisher, bound);
  cpppromise::Get(consumer.StartConsuming());

  for (int i = 0; i < large_message_count; i++) {
    cpppromise::Get(publisher.Publish());
  }

  publisher.Join();
  consumer.Join();

  ASSERT_EQ(consumer.received_.size(), bound);
}