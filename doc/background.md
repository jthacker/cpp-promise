# Background on CppPromise

### 1   Goals

One of the core challenges of building a correct C++ program is the correct handling of threads. The standard
library gives us some useful bits like `std::mutex`, expecting us to do _ad hoc_ checks wherever needed. In theory,
this is adequate, yet any C++ program of any size ends up containing data races. These are, in turn, notoriously
difficult to debug.

The CppPromise framework aims to make it easy to build correct C++ programs. It is not a solution for everyone; for
example, if you care about squeezing out every single cycle from your execution, you probably don't want to use it.
However, we believe it helps a broad category of programs that need to take advantage of multi-threading (e.g., to
work properly on multiple cores, or because they naturally have more than one thing going on at a time).

CppPromise is very deliberately based on concurrent programming concepts that have been popularized by
JavaScript. But to understand these, and the motivation for CppPromise, it is useful first to dive into their history.
Many programmers believe that the JavaScript model of concurrency was purpose-built, and associate it with Web pages.
We hope to convince you that this is not the case.

### 2   History and related work

In 1978, Tony Hoare pioneered a method of programming termed
[Communicating Sequential Processes](https://en.wikipedia.org/wiki/Communicating_sequential_processes) (CSP). According
to it, a general purpose concurrent program can be decomposed into a series of _processes_, each of which does only one
thing at a time, and communicates by sending messages to other processes. In this case, a _process_ represents a concept
within one program, rather than the more familiar idea of an operating system process. Languages like
[Go](https://go.dev/) take this idea towards its logical conclusion, where a _process_ can be a single function
invocation. Another, now-defunct language, [occam](<https://en.wikipedia.org/wiki/Occam_(programming_language)>), took it
even further; each _line of code_ was an independent process! Today, many developers are familiar with CSP and how it
is implemented by Go and similar languages.

Earlier, in 1973, Carl Hewitt _et al._ had pioneered a somewhat different idea:
the [actor model](https://en.wikipedia.org/wiki/Actor_model) of
programming. According to it, the unit of concurrency was a single _actor_ -- best understood as an _object_ in
contemporary object-oriented programming terminology. Each actor implicitly held a message queue, processed messages
one at a time, and emitted messages to other actors. In 1996, E. Dean Tribble _et al._ used this concept to build
[Joule](<https://en.wikipedia.org/wiki/Joule_(programming_language)>), where indeed every object was an actor with an
event queue. The experience of the Joule team (which included Doug Crockford, later to author
[JavaScript: The Good Parts](https://www.amazon.com/JavaScript-Good-Parts-Douglas-Crockford/dp/0596517742)) was that
they were no longer able to program. How do you iterate over the elements of a vector if each access of `vec[i]` and
`vec.length()` is an asynchronous message to `vec`, and `vec` can have processed any number of messages in between?

Backing off from this purity, in 1997, Mark Miller _et al._ created a follow-on language,
[E](<https://en.wikipedia.org/wiki/E_(programming_language)>). In E, objects are organized into collections called
_vats_. Each vat contains a set of objects and a single event queue, which processes incoming messages and delivers
them to recipient objects, one at a time, within the vat. Inside a vat, only one thread of control happens at a time,
and importantly, the normal rules of programming apply. Asynchronous messages can be sent between vats, containing
arbitrary data, but no state (i.e., memory) is shared between the vats. Event loop concurrency was not invented by E,
nor did it stop with E. We mention E's history here because it comes up later on in the story.

In 1995, Brendan Eich went on a 2-week coding bender and gave the world
[JavaScript](https://en.wikipedia.org/wiki/JavaScript). Here, each Web page owns a set of objects -- some
visible on the screen, and some only part of the underlying JavaScript program. Each Web page owns an event queue
and processed events in sequence, these events being things like the arrival of data from the network and mouse and
keyboard clicks by the user. This is very much the world of E vats -- except with only one vat.

As the Web browser model was extended, it became possible to send messages between Web pages
(e.g. via [`postMessage`](https://developer.mozilla.org/en-US/docs/Web/API/Window/postMessage)), and it also became
possible to construct "vats" which were not Web pages
(i.e., [Web Workers](https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Using_web_workers)). This allowed
JavaScript to finally reach the multi-vat expressiveness of E.

Due in large part to the direct influence of Mark Miller, JavaScript took on some more concepts from E; chief among them
is the now-familiar [`Promise`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise)
object. Modern JavaScript -- including the conveniences of the
[`async`/`await`](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/async_function) keywords
-- would be unthinkable without `Promise`s. A `Promise` is just what it means: a data structure representing the delivery
of some data to the program at a future time. And to make the idea of "a future time" specific, we shall say, during a
future turn of the event queue. The event queue of a vat proceeds by processing one event at a time, each such cycle
being called a "turn".

### 3   Promises versus channels

It is worth comparing the `Promise` model of asynchrony with the channel abstraction used in `occam` and Go. In
particular, the authors are quite familiar with the use of Go and its use within Google, as guided by its original
developers.

Communicating sequential processes with channels are a clean theoretical model of computation. However, in practice,
programs written using this model end up with a somewhat awkward pattern. The problem comes up when a process needs
to multiplex several possible sources of input, each of which shows up on a separate channel. The solution is some
kind of [`select`](https://gobyexample.com/select) operator that operates on channels similarly to how POSIX
[`select`](https://man7.org/linux/man-pages/man2/select.2.html) operates on file descriptors:

```go
while true {
  select {
    case value0 := <-chan0:
      // handle a new value from chan0 in value0
    case value1 := <-chan1:
      // handle a new value from chan1 in value1
  }
}
```

This means a programmer has to pre-declare this `while` loop somewhere dealing with all possible inputs. It makes the
program flow and call stack easy to understand, but at the expense of always requiring this centralized dispatcher.
And, of course, not all the incoming channels are active all the time, so we need a way to choose which channels we
should "bother" to `select` on at any given time.

So far, we arguably identified issues only of taste. Perfectly good programs can be written with this design
pattern. The problem is now what happens when we push this to its logical conclusion.

One of the adages known to Go programmers is:
[Avoid using a channel in a public interface](https://go.dev/talks/2013/bestpractices.slide#25). This flies in the face
of a naïve reading of the [example](https://gobyexample.com/channels) which shows channels used to communicate
between two Goroutines. The issue is that channels are complex; the semantics of who closes which end of a channel
and what happens as a result can create the same race conditions that the channels and Goroutines were designed to
solve in the first case. And so Go programmers are admonished to use channels only in internal implementations, and
to use plain blocking calls in public interfaces.

Perhaps as a result of this, the Go library does not attempt to use channels as a simple organizing abstraction.
[Reading a file](https://gobyexample.com/reading-files) is a blocking, synchronous activity. And programmers are
encouraged to use shared-state concurrency using constructs like [`sync.Mutex`](https://go.dev/tour/concurrency/9).

By comparison, in JavaScript, `Promise`s are extensively used in APIs. They have understandable semantics. Why is that?

The first thing to note is that a `Promise` is basically a one-shot channel. Since a `Promise` does not have to handle
the complexities of being a long-running object that has to handle potentially multiple error conditions within its
lifetime, and may become invalid or "closed" for multiple reasons, its API is simple enough to depend upon. In
particular, a`Promise` to supply a value means that the supplier is committed to supplying it even if the consumer
loses interest, and that is okay, because the wasted computation is small: If the consumer needs a new value, they will
*ask* for one by asking for a fresh `Promise` from the supplier.

As a one-shot channel, a `Promise` can be modeled using the CSP abstractions we are used to. So in some sense, we are
doing nothing "new" by introducing `Promise`s; we are merely making CSP more palatable. But there is another issue
lurking. Let's say we have an application which uses `Promise`s throughout. How would we write a `while` loop with a
`select` on all these `Promise`s?

```go
while true {
  select {
    case value0 := <-promise0:
      // handle a new value from promise0 in value0
    case value1 := <-promise1:
      // handle a new value from promise1 in value1
  }
}
```

This does not work, because the set of `Promise`s in the application changes at run-time. This is by design, and is a
feature, not a bug! So we need something that's not a static `select` -- we need to be able to say, "Whenever this
`Promise` resolves, do whatever is needed." The solution is to put the `while` loop into a generic class, and use it
as an event queue. In JavaScript, we say:

```javascript
aPromise.then((value) => {
   // do something with the value 
});
```

and the event queue on which the closure accepting `value` runs is implicit: there is only one, and it is global to a
JavaScript context.

We can definitely stop here. We now have a single-threaded program that reacts to data asynchronously delivered via
`Promise`s. But what if we want actual concurrency, to make use of multiple cores on our host machine?

In JavaScript, the solution is to create
[Web Workers](https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Using_web_workers)
and/or
[`<iframe>`s](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/iframe),
each of which contains its own JavaScript context and event loop, and sends messages to other contexts via an
asynchronous mechanism like [`postMessage`](https://developer.mozilla.org/en-US/docs/Web/API/Window/postMessage).

### 4   Existing C++ asynchronous frameworks

#### 4.1   The C++ standard library

The C++ standard library provides [`thread`](https://en.cppreference.com/w/cpp/thread/thread)
for creating threads of control, and several objects like
[`mutex`](https://en.cppreference.com/w/cpp/thread/mutex) for _ad hoc_ programmer controlled mutual exclusion.
This system does not help the programmer avoid data races.

The C++ library also provides [`promise`](https://en.cppreference.com/w/cpp/thread/promise) and
[`future`](https://en.cppreference.com/w/cpp/thread/future) objects, which act as a rendezvous between different
threads exchanging data. This is useable with [`async`](https://en.cppreference.com/w/cpp/thread/async) to
create asynchronous computations very similar to those one would encounter in, say, JavaScript or TypeScript. And as
long as these computations are pure functions with no side effects, one can indeed program confidently in this way.
But as soon as we introduce shared access to data, and the possibility of data races, we are back to raw use of
[`std::mutex`](https://en.cppreference.com/w/cpp/thread/mutex) and friends, under the control of the programmer.

#### 4.2   Boost Fiber

The Boost [Fiber](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/index.html) library
builds lightweight threads in userland and provides a series of utilities for inter-thread communication. The basic
[`fiber`](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/fiber/fiber_mgmt/fiber.html)
object is an analogue of a thread, with familiar methods like
[`join`](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/fiber/fiber_mgmt/fiber.html#fiber_join).

Fibers are [synchronized](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/fiber/synchronization.html) via
a number of mechanisms, some of which look like standard programmer controlled mutual exclusion primitives. There are,
however, two that deserve special mention.

Boost provides a
[`future`](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/fiber/synchronization/futures/future.html)
object which can be returned from a
[`promise`](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/fiber/synchronization/futures/promise.html).
The value of a `future` is obtained via functions like
[`get`](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/fiber/synchronization/futures/future.html#future_get),
or a client can
[`wait_for`](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/fiber/synchronization/futures/future.html#future_wait_for)
the value. The library includes a
[`async`](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/fiber/synchronization/futures/future.html#fibers_async)
function, which runs a supplied function in a new `fiber` and returns the result as a `future`. The `future`
abstraction does not, unfortunately, solve the data race problem.

Boost also provides a
[channel](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/fiber/synchronization/channels.html)
abstraction, which can be either a
[`buffered_channel`](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/fiber/synchronization/channels/buffered_channel.html)
or an
[`unbuffered_channel`](https://live.boost.org/doc/libs/1_84_0/libs/fiber/doc/html/fiber/synchronization/channels/unbuffered_channel.html).
The channel abstraction is, however, sorely missing a `select` primitive, making it unsuited to general purpose CSP
where a single process handles inputs from multiple sources.

In the end, it was the lack of a `select` that led us to not select the Boost Fiber utilities.

#### 4.3   RxCpp

The [RxCpp](https://github.com/ReactiveX/RxCpp) framework implements
[reactive programming](https://en.wikipedia.org/wiki/Reactive_programming) in C++. It does not provide any utilities
for creating multiple threads of control, but it has the concept of a "worker" and has functions like
[`observe_on_one_worker`](https://reactivex.io/RxCpp/classrxcpp_1_1observe__on__one__worker.html)
for reacting to new values in a user-defined thread of control.

The RxCpp model is not necessarily the "right" one for general-purpose asynchronous programming. Not everything is
an Observable, even though the model may be useful for some problems. The Observable model also does not help us
solve our main problem: mediating shared access to mutable state.

More worryingly, though, we encountered some segfaults when working with multiple threads, and found that we had
reproduced [Issue 595](https://github.com/ReactiveX/RxCpp/issues/595), which has received no attention. The last commit
to the RxCpp repo was 2 years old, at time of writing, and the RxCpp codebase is very complex.

#### 4.4   C++ CSP2

The University of Kent has a project called [C++ CSP2](https://www.cs.kent.ac.uk/projects/ofa/c++csp/) which implements
an `occam`-style CSP in C++. It does not seem to have received much work since 2007, so we are considering it a dead
project.

#### 4.5   The event loop model

In our research, we found a lot of precedent for the use of event loops in C++. We found special-purpose implementations
of event loops inside Chrome; in the Fuscia code; in server implementations within the Google3 codebase; and others.
Event loops are common in user interfaces, and are used in
[Qt](https://doc.qt.io/qt-6/eventsandfilters.html);
[Gnome](https://developer-old.gnome.org/clutter-cookbook/stable/events.html);
and [DBus](https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html).

### 5   Our approach

Our approach to solving the problem has tended towards providing an opinionated framework rather than a series of
independent functions. This is consistent with the basic inversion of control principle inherent in event loop
concurrent systems: As a programmer, you gain freedom by supplying your code and waiting to be called.

Our system, described in [The CppPromise Programming Manual](manual.md), requires the programmer to factor their
application -- or, at least, the portion of it they wish to use with CppPromise -- into a graph of cooperating
`Process` objects, each of which owns a thread of control. In that model, the `Promise` objects "just work", and
as long as the programmer follows some simple design patterns, data races cannot occur.
