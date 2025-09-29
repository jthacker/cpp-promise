# CppPromise stream implementation notes

## Introduction

CppPromise streams are intended to be an easy way for developers to implement pub/sub design patterns, integrated with the `EventQueue` and `Promise` abstractions familiar to regular CppPromise programmers.

## Basic constitutents

There are serveral basic classes of objects in the CppPromise streams implementation:

### Class `Topic`

A `Topic` is an object that a publisher creates in order to use the system. The `Topic::Publish` method publishes a new value and returns a `Promise<Empty>` that resolves when all recipients have finished processing the new value.

### Class `Publication`

As described in the programming documentation, a `Publication` is a "facet" of the `Topic` that a publisher can expose to clients. Clients call `Publication::Subscribe`, and the publisher need not worry about the details of how that works.

### Class `Subscription`

A `Subscription` is what a subscriber receives as a result of calling `Publication::Subscribe`. This is a simple value type -- it can be copied and used at will. It has two very important functions:

- When someone calls `Subscription::Unsubscribe`, no more values are sent to the client, _ever_. This happens with immediate effect. Once a client returns from `Subscription::Unsubscribe`, they are guaranteed never to receive any more values.

- When the last copy of the `Subscription` a client received from a `Publication::Subscribe` call goes out of scope, the effect is the same as if `Subscription::Unsubscribe` had been called.

We make the distinction between a `Subscription` object (a value type that can be copied at will) and a "subscription" (the whole fact of calling `Publication::Subscribe` and thus registering a new listener for published values).

### Struct `SubscriptionControlBlock`

This `struct` contains a bunch of data that describes the state of the "subscription" -- the "thing" that is made when a client calls `Publication::Subscribe`. It is deliberately made a `struct` with no member functions because it has no independent behavior; the other objects use it to coordiante their activities. Among the information it contains is:

- A `std::mutex` to coordinate the behaviors of the "subscription".

- The callback function `std::function<void(T)>` that the client supplied in `Publication::Subscribe`.

- The `EventQueue` that the client is using, because this is where new published events will be delivered.

- A pointer to the `Topic` that produced this "subscription". This is important to keep around in order to implement `Subscription::Unsubscribe`.

Various entities hold `std::shared_ptr`s to the `SubscriptionControlBlock`, and the way in which these work to ensure that we do not leak memory, but that we also do not de-allocate memory prematurely, is really important to understand.

### Class `UnsubscribeTrigger`

An `UnsubscribeTrigger` is a tiny object whose _only_ job is to be the target of a bunch of `std::shared_ptr`s. When these go out of scope, the destructor of `UnsubscribeTrigger` is called, which in turn does the same work that `Subscription::Unsubscribe` would do.

## Subscribing to a `Topic`

When a client calls `Publication::Subscribe`, the `Publication` object creates a new `SubscriptionControlBlock` and the rest of the useful objects, and wires up all the necessary connections:

![](scbCreation.excalidraw.png)

Later on, as the client passes copies of the `Subscription` object around, there will be multiple `Subscription`s:

![](scbInUse.excalidraw.png)

## Publishing a new value

When the publisher calls `Topic::Publish`, the `Topic` creates a new event (a lambda) in the client's event queue. This event contains (yet another) `std::shared_ptr` to the `SubscriptionControlBlock`. It also contains the published value of the appropriate type `T`.

![](scbPublishValue.excalidraw.png)

## Delivering an event

Eventually, the recipient's event loop will execute the event created by `Topic::Publish`. The lambda in the event will check the `SubscriptionControlBlock` to make sure that its pointer to its `Topic` is not `nullptr`. If that is the case, it calls the listener function supplying the published value of the appropriate type `T`.

![](scbDeliverEvent.excalidraw.png)

## Unsubscribing

There are two ways to unsubscribe. One is to call the `Subscription::Unsubscribe` function, and the other is if all the `Subscription`s go out of scope, causing the `UnsubscribeTrigger`'s dtor to be called. The underlying behavior is the same, so let's consider the second one. In that case, the destructor of the `UnsubscribeTrigger` nulls out the `Topic` pointer in the `SubscriptionControlBlock`, and disconnects it from the `Topic`:

![](scbUnsubscribe.excalidraw.png)

The result is the following structure. Note that the event(s) waiting in the recipient's event queue are still waiting, and each of them still holds a `std::shared_ptr` to the `SubscriptionControlBlock`. When each of these events is executed, it will note that the `Topic` pointer in the `SubscriptionControlBlock` is `nullptr`, and will simply do nothing. Once the last such event, and thus the last `std::shared_ptr`, goes out of scope, the `SubscriptionControlBlock` will be destructed.

![](scbPostUnsubscribe.excalidraw.png)

This behavior is really important. It is how we are sure that, if `Subscription::Unsubscribe` is called or the last `Subscription` goes out of scope, the stream of events is *synchronously* halted -- from that point on, the recipient can be sure they will receive no further events. The recipient can now tear down any resources they were using to handle new events.
