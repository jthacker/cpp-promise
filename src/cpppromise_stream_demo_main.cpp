#include <iostream>
#include <optional>

#include "cpppromise.h"
#include "cpppromise_stream.h"

using namespace cpppromise;

class PublisherProcess : public Process {
public:
  PublisherProcess(int max_count) : count_(0), max_count_(max_count) {}

  void Start() {
    Enqueue([this]() {
      SendCount();
    });
  }

  Publication<int>& GetCounter() {
    return count_topic_.GetPublication();
  }

  Promise<Empty> WhenDone() {
    return EnqueueWithResolver<Empty>([this](Resolver<Empty> r) {
      done_.push_back(r);
    });
  }

private:
  void SendCount() {
    if (count_ >= max_count_) {
      for (auto &r: done_) {
        r.Resolve(Empty());
      }
      done_.clear();
      Finish();
      return;
    }

    int current_count = count_;

    count_topic_.Publish(current_count)
        .Then([current_count]() {
          std::cout << "Publisher finished publishing " << current_count << std::endl;
        });

    count_++;

    Enqueue([this]() {
      SendCount();
    });
  }

  Topic<int> count_topic_;
  int count_;
  int max_count_;
  std::vector<Resolver<Empty>> done_;
};

class SubscriberProcess: public Process {
public:
  SubscriberProcess(PublisherProcess* publisher) : publisher_(publisher) {
    Enqueue([this]() {
      subscription_ = publisher_->GetCounter().Subscribe([this](int k) {
        std::cout << "Callback subscriber got value " << k << std::endl;
      });
      publisher_->WhenDone().Then([this]() {
        if (subscription_) {
          subscription_->Unsubscribe();
          subscription_.reset();
        }
        Finish();
      });
    });
  }

private:
  PublisherProcess* publisher_;
  std::optional<Subscription<int>> subscription_;
};

class RootProcess : public Process {
public:
  RootProcess() {
    publisher_ = std::make_unique<PublisherProcess>(5);
    Enqueue([this]() {
      Start();
    });
  }

  void Join() override {
    publisher_->Join();
    Process::Join();
  }

private:
  void Start() {
    publisher_->Start();
    publisher_->WhenDone().Then([this]() {
      Finish();
    });
  }

  std::unique_ptr<PublisherProcess> publisher_;
  std::unique_ptr<SubscriberProcess> subscriber_;
};

void test_publisher_only() {
  std::cout << "test_publisher_only" << std::endl;
  PublisherProcess p(1);
  p.Start();
  p.Join();
}

void test_all() {
  std::cout << "test_all" << std::endl;
  RootProcess root;
  root.Join();
}

int main(int argc, char** argv) {
  test_publisher_only();
  test_all();
  std::cout << "done" << std::endl;
}

