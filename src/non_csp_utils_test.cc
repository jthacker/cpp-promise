#include "src/cpp_common/cpppromise/non_csp_utils.h"

#include "gtest/gtest.h"

namespace cpppromise {

TEST(NonCSPUtilsTest, Get) {
  int var = 0;
  EventQueue q;
  {
    Promise<int> p = q.Enqueue<int>([]() { return 1; });
    var = Get(p);
  }

  ASSERT_EQ(1, var);

  q.Finish();
  q.Join();
}

TEST(NonCSPUtilsTest, GetAsyncFunc) {
  int var = 0;
  EventQueue q;

  {
    var = Get<int>([&]() {
      return q.Enqueue<int>([]() { return 1; }).Then<int>([](int res) {
        return res + 1;
      });
    });
  }

  ASSERT_EQ(2, var);

  q.Finish();
  q.Join();
}

TEST(NonCSPUtilsTest, SubscribeAndWait) {
  EventQueue q;

  std::vector<int> received;

  Topic<int> topic;
  Publication<int> publication = topic.GetPublication();

  bool stop = false;
  int count = 0;
  std::function<void()> keep_publishing = [&]() {
    q.Enqueue([&]() {
      if (stop) {
        return;
      }
      topic.Publish(count++);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      keep_publishing();
    });
  };
  keep_publishing();

  SubscribeAndWait<int>(publication, [&](int data) {
    if (received.size() < 5) {
      received.push_back(data);
      return true;
    }

    return false;
  });

  q.Enqueue([&] { stop = true; });

  ASSERT_EQ(received.size(), 5);

  int pre = received[0];
  for (int i = 1; i < 5; i++) {
    ASSERT_EQ(received[i], pre + 1);
    pre = received[i];
  }

  q.Finish();
  q.Join();
}
}  // namespace cpppromise