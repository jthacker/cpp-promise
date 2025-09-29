# CppPromise implementation notes

## Introduction

The whole idea of CppPromise is to help developers write deadlock-free multi-threaded code without worry. Users can create entire programs without ever touching a `std::mutex` or other similar utility. And yet, to create this framework, we need to do these deadlock-prone operations _within_ the framework. The advantage is that we do them in one place, where they can be tested extensively, rather than doing them all over the application code. But the risk is that small errors in the framework can manifest subtly in applications. As a result, we need to get things right.

## Basic constitutents

There are two basic classes of objects in the CppPromise implementation, and some miscellaneous entities.

### Class `EventQueue`

An `EventQueue` is a thread of execution that executes zero-argument `std::function`s.

- A client can `Enqueue` a function to be called in the `EventQueue`'s thread.
- A client can call `Take`, which increments a "lease" preventing the `EventQueue` from shutting down. The client can call `Release` later on to decrement the lease.

### Class `PromiseControlBlock`

A `PromiseControlBlock` is the central unit of data flow coordination. A `PromiseControlBlock` may be associated with an `EventQueue` that it uses to perform work when needed. The `Promise` and `Resolver` objects exposed to users are just smart pointers referring to a `PromiseControlBlock`. The `Promise::Then` family of methods creates data dependencies between `PromiseControlBlock`s.

### Support classes

The remainder of the material in CppPromise is just there to support the above two classes. A `Process` is just a convenience wrapper around an `EventQueue`. Class `Timer` is used to schedule repeated events, and is a very simple singleton.

## Locking in an `EventQueue`

Each `EventQueue` has a `std::mutex mu_` protecting its state. All public methods of `EventQueue` lock `mu_`. However, there are two _very_ important implementation invariants, without which the framework is not safe.

### Operations with locks on `mu_` do not propagate

Any operation that locks `mu_` does not then proceed to do something else that might try to grab another lock. All operations on the `EventQueue` proceed as follows:

```
* Lock `mu_`;
* Mutate _local_ state in the `EventQueue` only; and
* Release `mu_` and return.
```

Anyone can therefore call an `EventQueue` method and be guaranteed that this method _will_ make progress and return.

### No locks when running user code

As the `EventQueue` executes user code (the stuff inside a supplied `std::function`), it does _not_ hold onto its own lock `mu_`. This is very important because it ensures that the user code can do anything it wants to -- including calling operations on this `EventQueue` which acquire a lock on `mu_`. The `EventQueue`'s worker thread is a single `while` loop, and we encourage the reader to examine its implementation carefully and note how it achieves this invariant.

The general design pattern here is that, instead of doing:

```
while (true) {
  std::unique_lock<std::mutex> lock(mu_);
  if (this->something_is_true_) {
    RunSomeUserCode(); // <-- mu_ is still locked!
  }
}
```

we do:

```
while (true) {
  bool something_is_true;

  {
    // Query and save information while mu_ is locked, then release it
    std::unique_lock<std::mutex> lock(mu_);
    something_is_true = this->something_is_true_;
  }

  if (something_is_true) {
    RunSomeUserCode(); // <-- mu_ is not locked here
  }
}
```

## Locking in and among `PromiseControlBlock`s

Each time a user calls `Promise::Then`, a data dependency is created between two `PromiseControlBlock`s. An example of the data structure thus created is the following:

![](pcbTree.excalidraw.png)

Notice that this structure is a **tree** by construction. The process of resolving data dependencies will traverse this tree from top to bottom, grabbing the lock of each `PromiseControlBlock` it traverses, and maybe also grabbing a lock on an associated `EventQueue`:

![](pcbLocks.excalidraw.png)

Since the `PromiseControlBlock`s are arranged as a tree, locks that proceed along the data dependency as illustrated above will not be cyclic, and so are not at risk of deadlock. Since the `EventQueue` locking operations complete without acquiring any other locks, they are safe when called from the process of resolving dependencies down the `PromiseControlBlock` tree. And since the original code that provided the resolving value was running in some kind of user function, it is not currently holding any `EventQueue` locks. As a result, these system is safe.
