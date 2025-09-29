#include <iostream>
#include <sstream>

#include "cpppromise.h"

using namespace cpppromise;

class MotdProcess : public Process {
public:
  MotdProcess()
      : next_count_(0),
        messages_ {
            "You’re braver than you believe, and stronger than you seem, and smarter than you think.",
            "Keep your face to the sunshine and you cannot see a shadow.",
            "In every day, there are 1,440 minutes. That means we have 1,440 daily opportunities to make a positive impact.",
            "The only time you fail is when you fall down and stay down.",
            "Positive anything is better than negative nothing.",
            "Optimism is a happiness magnet. If you stay positive good things and good people will be drawn to you.",
            "Happiness is an attitude. We either make ourselves miserable, or happy and strong. The amount of work is the same.",
            "It’s not whether you get knocked down, it’s whether you get up.",
            "The struggle you’re in today is developing the strength you need tomorrow.",
            "Happiness is the only thing that multiplies when you share it.",
            "The happiness of your life depends upon the quality of your thoughts.",
            "Once you replace negative thoughts with positive ones, you’ll start having positive results.",
            "Positive thinking will let you do everything better than negative thinking will.",
            "The way I see it, if you want the rainbow, you gotta put up with the rain.",
            "The more you praise and celebrate your life, the more there is in life to celebrate.",
            "If you want light to come into your life, you need to stand where it is shining.",
            "The good life is a process, not a state of being. It is a direction, not a destination.",
            "A truly happy person is one who can enjoy the scenery while on a detour.",
            "You’re off to great places, today is your day. Your mountain is waiting, so get on your way.",
            "Winning doesn’t always mean being first. Winning means you’re doing better than you’ve done before.",
            "Winning is fun, but those moments that you can touch someone’s life in a very positive way are better.",
            "Virtually nothing is impossible in this world if you just put your mind to it and maintain a positive attitude.",
            "You are never too old to set another goal or dream a new dream.",
            "Every day may not be good… but there’s something good in every day.",
            "The difference between ordinary and extraordinary is that little extra.",
            "Be so happy that, when other people look at you, they become happy too.",
            "No one is perfect – that’s why pencils have erasers.",
            "Let your unique awesomeness and positive energy inspire confidence in others.",
            "Wherever you go, no matter what the weather, always bring your own sunshine.",
            "When we are open to new possibilities, we find them. Be open and skeptical of everything.",
            "Live life to the fullest and focus on the positive.",
            "You always pass failure on the way to success.",
            "It always seems impossible until it is done.",
            "When you are enthusiastic about what you do, you feel this positive energy. It’s very simple.",
            "It makes a big difference in your life when you stay positive.",
            "If opportunity doesn’t knock, build a door.",
            "The sun himself is weak when he first rises, and gathers strength and courage as the day gets on.",
            "Hard work keeps the wrinkles out of the mind and spirit.",
            "Success is the sum of small efforts repeated day in and day out.",
        } {}

  ~MotdProcess() override { Finish(); Join(); };

  Promise<std::string> get_next_message() {
    return Enqueue<std::string>([this]() {
      auto s = messages_[next_count_];
      next_count_ = (next_count_ + 1) % messages_.size();
      return s;
    });
  }

  Promise<std::string> get_hello_message() {
    return EnqueueWithResolver<std::string>([this](Resolver<std::string> resolver) {
      resolver.Resolve("Hello to you nice processes");
    });
  }

private:
  int next_count_;
  std::vector<std::string> messages_;
};

class ReporterProcess : public Process {
public:
  ReporterProcess(MotdProcess* motd) : motd_(motd), running_(true) {
    Enqueue([this]() {
      get_next();
    });
  }

  ~ReporterProcess() override { Finish(); Join(); }

  void cease() {
    Enqueue([this]() {
      running_ = false;
    });
  }

private:
  void get_next() {
    motd_->get_next_message()
        .Then<std::string>([](std::string s) {
          std::cout << "got message: " << s << std::endl;
          return s;
        })
        .Then<int>([](std::string s) {
          return s.length();
        })
        .Then([](int k) {
          std::cout << "string length was " << k << std::endl;
        })
        .Then([this]() {
          if (running_) {
            get_next();
          }
        });
    motd_->get_hello_message()
        .Then([](std::string s) {
          std::cout << "got hello message: " << s << std::endl;
        });
  }

  MotdProcess* motd_;
  bool running_;
};

void test0() {
  EventQueue q0;
  q0.Enqueue([&q0]() {
    auto promise_resolver_pair = q0.CreateResolver<int>();
    promise_resolver_pair.second.Resolve(42);
  });
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

void test1() {
  MotdProcess p1;
  ReporterProcess p2(&p1);
  std::this_thread::sleep_for(std::chrono::seconds(5));
  p2.cease();
}

void test2() {
  EventQueue q;

  for (int i = 0; i < 100; i++) {
    Promise<int> a = q.Enqueue<int>([i]() {
      std::cout << "first Job producing " << i << std::endl;
      return i;
    });

    Promise<std::string> b = a.Then<std::string>(&q, [](int x) {
      std::cout << "second Job received " << x << std::endl;
      std::ostringstream os;
      os << "[" << x << "]";
      std::cout << "second Job producing " << os.str() << std::endl;
      return os.str();
    });

    Promise<int> c = b.Then<int>(&q, [](std::string z) {
      std::cout << "third Job received " << z << std::endl;
      int k = z.length();
      std::cout << "third Job producing " << k << std::endl;
      return k;
    });

    Promise<Empty> d = q.Enqueue<Empty>([i]() {
      std::cout << "fourth Job having " << i << std::endl;
      return Empty();
    });

    Promise<Empty> e = d.Then<Empty>(&q, [i](Empty) {
      std::cout << "fifth Job having " << i << std::endl;
      return Empty();
    });
  }
}

int main(int argc, char** argv) {
  test0();
  test1();
  test2();
  std::cout << "done" << std::endl;
}