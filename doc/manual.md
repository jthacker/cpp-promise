# The CppPromise Programming Manual

## Your first process

The core design of any CppPromise project is its breakdown into processes. To do that, you first need to know how to
make one. Here's how:

```c++
#include <cpppromise.h>

using cpppromise;

int main(int argc, char** argv) {
  Process p;
}
```

That's it! This program will do nothing and exit. To do something more useful, you need to build a subclass of `Process`
that does some work. Here's one way:

```c++
#include <cpppromise.h>
#include <iostream>

using cpppromise;

class CounterProcess: public Process {
 public:
  CounterProcess() : count_(0) {
    Enqueue([this]() {
      PrintOne();  
    });
  }
  
 private:
  void PrintOne() {
    std::cout << count_ << std::endl;
    if (count_ < 10) {
      Enqueue([this]() {
        PrintOne();
      });
    }
    std::cout << count_ << std::endl;
    count_++;
  }  
  
  int count_;
};

int main(int argc, char** argv) {
  CounterProcess p;
}
```

This prints all the numbers between `0` and `9`, printing each one twice, then exits. The way it works, the `CounterProcess` ctor is first
called; it `Enqueue`s some work and returns immediately. The `Enqueue` function schedules the work at the end of the
`CounterProcess`'s (currently empty) event queue. Each bit of work causes the next bit of work to be `Enqueue`ed, until
the stopping condition is met.

The output should provide a clue as to the execution model. The call to `Enqueue` inside `PrintOne` is not recursive.
The `PrintOne` function _runs to completion_ before the next version of it is called.

At this point, you will likely wonder how it is that `main` "knew" to exit after the last numbers were written out.
Well, it simply called the destructor of `CounterProcess`, which blocked on the completion of all the pending work.
Since each `PrintOne` made sure to `Enqueue` its successor before it exited, there was always work to do. When the
stopping condition was met, there was nothing more to do, and the `CounterProcess` destructor returned.

This is dangerous, though! What _really_ happened is that the thread of `main` was executing the `CounterProcess`
destructor for most if not all the lifetime of the program. In fact, it blew right past the `CounterProcess` destructor
and blocked inside the destructor of `Process`. If the destructor of `CounterProcess` had de-allocated some important
resources, the work in the queue would have crashed!

This leads us to a very important design pattern: _always_ arrange for your `Process` to be shut down cleanly. A
correct `CounterProcess` would look like this:

```c++
#include <cpppromise.h>
#include <iostream>

using cpppromise;

class CounterProcess: public Process {
 public:
  CounterProcess() : count_(0) { /* ... */ }

  void Join() override {
    Finish();
    Process::Join();
  }
  
  /* ... */
};

int main(int argc, char** argv) {
  CounterProcess p;
}
```

The recommended design pattern is to override the `Join` member function. Here, you should send yourself any messages
that you need to make sure your `Process` is shut down. You `Finish`, ensuring that your event queue will exit when
there is no more work to do. Finally, you call your superclass `Join`, forcing the caller to wait until all that is
complete. In this sense, the `Join` method works just like `std::thread::join` in that it blocks the calling thread
until the `Process` is done with its work.

From the perspective of the _client_ of a `Process`, the right thing to do to wait for a `Process` to be done is
to `Join` it:

```c++
#include <cpppromise.h>
#include <iostream>

using cpppromise;

class CounterProcess: public Process {
 public:
  CounterProcess() : count_(0) { /* ... */ }
  
  void Join() override {
    Finish();
    Process::Join();
  }
  
  /* ... */
};

int main(int argc, char** argv) {
  CounterProcess p;
  p.Join();
}
```

Your process can also call `Finish` on itself when there is no more work to do. For example:

```c++
#include <cpppromise.h>
#include <iostream>

using cpppromise;

class CounterProcess: public Process {
 public:
  CounterProcess() : count_(0) {
    Enqueue([this]() {
      PrintOne();  
    });
  }
  
  void Join() override {
    Finish();  // just in case
    Process::Join();
  }
  
 private:
  void PrintOne() {
    std::cout << count_ << std::endl;
    if (count_ < 10) {
      Enqueue([this]() {
        PrintOne();
      });
    } else {
      Finish();
    }
    std::cout << count_ << std::endl;
    count_++;
  }  
  
  int count_;
};

int main(int argc, char** argv) {
  CounterProcess p;
  p.Join();
}
```

In general, as you are designing your `Process`, if you expect clients to wait for it to complete, you should plan for
that in your `Join` function. You may even want to design a custom protocol where other parts of the system inform you
of the intent to shut down. We will talk later about how `Process` objects can exchange information. For now, to
summarize, let's list all the various "get done" member functions on class `Process` and what they are used for:

| Function | Visibility  | Purpose                                                                                                         |
| -------- | ----------- | --------------------------------------------------------------------------------------------------------------- |
| `Finish` | `protected` | Called from subclass to indicate that, once the queue is empty, this `Process` is complete.                     |
| `Join`   | `public`    | Called from external client to wait for this `Process` to complete. Overridden by subclass to do cleanup first. |

These tools allow you to create a reliable `Process` having correct communication with the outside world (we'll get to
that next), and a completely predictable shutdown sequence. You can therefore create and destroy `Process` instances
in your program with impunity! By the time you count up all the _ad hoc_ work you have to do to get raw `std::thread`s
sharing state politely (which, again, we'll talk about soon), we believe using the `Process` protocol will seem really easy.

## Promises and resolvers

When you `Enqueue` an operation, you obviously do not have access to its result immediately. In theory, you could supply
a function to be called later on when the result is available. The `Process` gives you a much more succinct and
expressive representation, a `Promise`. This concept is deliberately modeled after the
[JavaScript `Promise`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise) object,
so everything you know about the subject from JavaScript should be applicable here. Here is a trivial example:

```c++
#include <cpppromise.h>
#include <iostream>

using cpppromise;

class CounterProcess: public Process {
 public:
  CounterProcess() : count_(0) {
    Enqueue([this]() {
      DoSomething();
    });
  }
  
 private:
  void DoSomething() {
    Promise<int> a = Enqueue<int>([]() {
      count_++;
      std::cout << count_ << std::endl;
      return count_;
    });
    Promise<float> b = a.Then<float>([](int k) {
      std::cout << (k + 1) << std::endl;
      return (k + 2) + 0.05f;
    });
    b.Then([](float k) {
      std::cout << k << std::endl;
    });
    std::cout << count_ << std::endl;
  }
  
  int count_;
};
```

What does this print? If you guessed the following, you are right:

```text
0
1
2
3.05
```

The `Promise<T>` is exactly what its name says: a promise for the future delivery of a value of type `T`. Calling
`Then` schedules a function to be called with the result. This in turn returns a `Promise` which can be used in the
same way. In fact, the code in `DoSomething` is idiomatically abbreviated to:

```c++
  void DoSomething() {
    Enqueue<int>([]() {
      count_++;
      std::cout << count_ << std::endl;
      return count_;
    })
    .Then<float>([](int k) {
      std::cout << (k + 1) << std::endl;
      return (k + 2) + 0.05f;
    })
    .Then([](float k) {
      std::cout << k << std::endl;
    });
    std::cout << count_ << std::endl;
  }
```

The concept of a `Promise` is really "just" a bunch of enqueued callback functions all chained together. But from
experience with the similar object in JavaScript, the ability to represent the future delivery of a value in such a
clear and portable way, and in particular, to pass the future delivery around in your code at will until it gets
to the point where it needs to be consumed, is transformative.

A `Promise` is a simple value with clean copy semantics. A copy of a `Promise` represents the same eventual delivery;
any `Then` on it is executed identically to a `Then` on the original. The framework takes care of memory managing the
values. Just remember that the type `T` in `Promise<T>` needs to also have copy semantics, because the result of a
computation could be delivered to multiple recipients, requiring copies to be made. If your data type `MyType` contains
some expensive resources or cannot easily be copied, then consider passing around `T = std::shared_ptr<MyType>` instead.

An operation that does not return anything produces a `Promise<Empty>`, indicating the completion of the operation and
nothing else. Class `Empty` is an empty placeholder. Template overloads in classes `Process` and `Promise` ensure that
you can write code expressively.

Sometimes you don't have an operation ready to enqueue, but you know you want to give someone a `Promise` for something.
Perhaps you don't know what kind of computation you are going to run, but you know it should produce some type `T`.
Rather than enqueueing some complicated logic with lots of conditionals and subsidiary enqueued operations, you can
just twirl your wand and create a `Promise` now, and worry about paying off the debt later. Here's how:

```c++
#include <cpppromise.h>
#include <iostream>

using cpppromise;

class MyProcess : public Process {
  // Much detail elided
  
 private:
  void DoSomething() {
    std::pair<Promise<int>, Resolver<int>> promise_resolver_pair = CreateResolver<int>();
    waiting_ = promise_resolver_pair.second;
    Promise<int> p = promise_resolver_pair.first;
    // Use "p" for something ...
  }
  
  void DoSomethingLater() {
    int result = GetSomeResult();
    if (waiting_.has_value()) {
      // Someone is waiting for something
      waiting_->Resolve(result);
      waiting_.reset();
    }
  }
  
  std::optional<Resolver<int>> waiting_;
};
```

Now, in JavaScript, a `Promise` gives you a `resolve` and a `reject` function. JavaScript `Promise`s have built-in
handling for an error condition. It really helps when chaining `Promise`s, especially using the `async` and `await`
keywords. With no such need in C++, we do not represent errors in our `Promise`s. And at the moment, we do not support
exception handling. If you need to pass error conditions, then use an `absl::StatusOr` or similar.

At this point, `Promise`s and `Resolver`s might seem cute, but they are not super useful. I mean, you can write a
`Process` that promises things to itself. Hardly anything to write home about. But things are about to change.

## Process collaboration and entry points

From previous chapters, we have seen how, with appropriate arrangements, you can structure your entire program as a
single `Process`. Of course, this limits your program to only one sequence of events. To take advantage of
preemptive multitasking, you need to create several `Process` objects and coordinate their activities. To do this, in
turn, you must know how to make the services of one `Process` accessible from others.

Consider a `Process` which provides an incrementing count. We could create it as follows:

```c++
#include <cpppromise.h>
#include <iostream>

using cpppromise;

class CounterProcess : public Process {
 public:
  CounterProcess(int increment) : count_(0), increment_(increment) {}
  
  Promise<int> GetCount() {
    return Enqueue<int>([this]() {
      int r = count_;
      count += increment;
      return r;
    });
  }

 private:
  int count_;
  int increment_;
};
```

`GetCount` is an entry point; it is safely callable by any other `Process`. It returns a `Promise` for the result of a
computation that is `Enqueue`d as part of the `CounterProcess`. All calls to `GetCount` will be serialized, with each
caller getting a unique value. There is no synchronization needed. Another `Process` could use it like this:

```c++
#include <cpppromise.h>
#include <iostream>

using cpppromise;

class CounterClientProcess : public Process {
 public:
  CounterProcess() : counter_(new CounterProcess(5)) {
    Enqueue([this]() {
      AskForCount();
    });
  }
  
  ~CounterProcess() {
    Stop();
  }
  
 private:
  void AskForCount() {
    counter_->GetCount().Then([](int k)) {
      std::cout << "Got count: " << k << std::endl;
      if (k >= 50) {
        Finish();
      } else {
        Enqueue([this]() {
          AskForCount();
        });
      }
    });
  }
  
  std::unique_ptr<CounterProcess> counter_;  
};
```

`CounterClientProcess` will simply run in a loop, asking for a new count from its `CounterProcess` as soon as
it gets a reply. It will count up from `0` by `5` up to `50`. Notice how `CounterClientProcess` casually uses the
returned `Promise`. This is the real purpose of `Promise`s -- to permit `Process` objects to communicate tersely and
correctly.

Note how the constructors of `CounterProcess` and `CounterClientProcess` all run in some unknown calling thread. We
should never do "real work" in our `Process` constructors; we should only save off the supplied parameters, then send a
message to ourselves to start our work.

We know from the API of `CounterProcess` that it does not publish any kind of protocol to prepare it for its destructor
to be safely called. We therefore just need a stopping condition in `CounterClientProcess`, and we call `Finish` when
that stopping condition is met. The call to `Finish` will un-block any `Join`ers of the `CounterClientProcess`, and the
call to `Stop` will ensure that the destructor is held up until all pending events are processed.

Sometimes, you need to provide a `Promise` to another `Process`, but you don't have a way to resolve it yet. In the
previous chapter, we learned how to use `CreateResolver` to do that. If you are giving the `Promise` away in an entry
point, though, you _may not_ use `CreateResolver`! To see why, consider this code:

```c++
#include <cpppromise.h>
#include <iostream>

using cpppromise;

class ExampleProcess : public Process {
 public:
  Promise<int> GetSomething() {
    auto promise_resolver_pair = CreateResolver<int>();  // WRONG!!
    // In what thread does this code run?
    return promise_resolver_pair.first;
    // How do we store promise_resolver_pair.second safely?
  }
};
```

The code will run in the thread of the _calling_ `Process`, not that of `ExampleProcess` where it belongs. This is
obviously wrong, because that code needs to modify the state of `ExampleProcess`. As it stands, this code will fail
immediately. Instead, you should use the companion to `CreateResolver`, called `EnqueueWithResolver`:

```c++
#include <cpppromise.h>
#include <iostream>

using cpppromise;

class ExampleProcess : public Process {
 public:
  Promise<int> GetSomething() {
    std::cout << "0" << std::endl;
    auto r = EnqueueWithResolver<int>([this](Resolver<int> r) {
      std::cout << "1" << std::endl;
      // This code runs in the event queue of ExampleProcess
    });
    std::cout << "2" << std::endl;
    return r;
  }
};
```

Because the closure passed to `EnqueueWithResolver` is scheduled on a separate event queue, this code might print
something like:

```text
0
2
1
```

Now you can see why we would not want to confuse this with `CreateResolver`! The sequence of operations is very
different.

To summarize, the correct way to create an entry point that uses a `Resolver` (without the extra explanatory
printing to `std::cout`) is:

```c++
#include <cpppromise.h>
#include <iostream>

using cpppromise;

class ExampleProcess : public Process {
 public:
  Promise<int> GetSomething() {
    return EnqueueWithResolver<int>([this](Resolver<int> r) {
      // This code runs in the event queue of ExampleProcess
      // Arrange to resolve the returned Promise via Resolver "r"
    });
  }
};
```

One question you may ask is, why is this not a problem in JavaScript? Well, JavaScript has only one (logical) event
queue for the entire program. To send messages to other programs running on different event queues, you need to use
a completely different API, like [`postMessage`](https://developer.mozilla.org/en-US/docs/Web/API/Window/postMessage).
In some sense, then, we are merely following the precedent of JavaScript!

## Working with streams of data

Sometimes, a `Process` needs to publish a _stream_ of information to a consumer. This is always possible to do with a
more low-level pattern, using some kind of custom object that the publisher can use to send information back to the
consumer. But since this is super common, CppPromise provides a canned pub/sub system.

To provide a stream of data, your publisher process needs to create a `Topic`, and call the `Publish` function when
it has something to offer. That's it.

```c++
#include <cpppromise.h>
#include <cpppromise_stream.h>
#include <iostream>

using cpppromise;

class PublisherProcess : public Process {
 public:
  PublisherProcess(int limit_count) : limit_count_(limit_count), count_(0) {
    Enqueue([this]() {
      SendNumber();
    });
  }
  
 private:
  void SendNumber() {
    if (count_ == limit_count_) {
      Finish();
      return;
    }
    numbers_topic_.Publish(count_++).Then([]() {
      std::cout << "Published one" << std::endl;
    });
    Enqueue([this]() {
      SendNumber();
    })
  }

  int limit_count_;
  int count_;
  Topic<int> numbers_topic_; 
};
```

Note first that we need to include an additional header, `cpppromise_stream.h`.

The `Publish` function returns a `Promise<Empty>` that resolves when all the subscribers have processed their results.
This can be handy if you want to rate-limit your stream, to make sure you don't overwhelm your subscribers with too much
data. If you do this, though, you should arrange this explicitly as part of your contract between you and your
subscribers. By default, a subscriber should assume that a publisher will send data as fast as it wants to. If the
subscriber can't keep up, it should implement some sort of defensive pattern. (More on that later.)

The work needed to expose a `Topic` to the outside world is really a bunch of boilerplate. To help with that, the
`Topic` exposes a helper object, an instance of a class called `Publication`, that is safe to expose to the outside
world. To create an entry point for clients to use your `Topic`, just expose a reference to its `Publication`:

```c++
#include <cpppromise.h>
#include <cpppromise_stream.h>
#include <iostream>

using cpppromise;

class PublisherProcess : public Process {
 public:
  // Much detail elided
  
  Publication<int>& GetNumbers() { return numbers_topic_.GetPublication(); }
  
 private:
  // Much detail elided
  
  Topic numbers_topic_; 
};
```

That's it. Assuming your clients do the right thing, you don't need to worry about any cleanup or
housekeeping. The `Topic` will do what's needed.

From the point of view of a client, your job is also pretty easy:

```c++
#include <cpppromise.h>
#include <cpppromise_stream.h>
#include <iostream>
#include <optional>

using cpppromise;

class SubscriberProcess : public Process {
 public:
  SubscriberProcess(PublisherProcess* publisher_) : publisher_(publisher) {
    Enqueue([this]() {
      subscription_ = publisher_->GetNumbers().Subscribe([](int k) {
        std::cout << "Callback got " << k << std::endl;  
      });
    });
  }
  
  void UnsubscribeFromSubscriber() {
    if (subscription_) {
      subscription_->Unsubscribe();
      subscription_.clear();
    }
    Enqueue([this]() {
      // At this point, we can safely assume no more messages will come in
    });
  }
  
 private:
  std::optional<Subscription<int>> callback_subscription_;
  PublisherProcess* publisher_;
};
```

The `Subscribe` function returns a `Subscription`, which we can use to `Unsubscribe` later. Note the pattern
in the example code for when it's safe to assume no more messages will come in. First call `Unsubscribe`, then _enqueue_
an event to yourself telling yourself to tear down any resources the subscription depends on. This makes sure that any
pending data published in your event queue is flushed before you start de-allocating stuff!
