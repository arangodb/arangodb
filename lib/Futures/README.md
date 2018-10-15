# Futures

A simple framework to perform async operations using a future / promise pattern.

## Overview

This framework is inspired by [Folly Futures](https://github.com/facebook/folly), but is greatly simplified for use in the ArangoDB server.
Compared to the C++11 futures it is much more powerful.

The primary difference from `std::future` is that you can attach callbacks to Futures (with `then()`), under the control of our 
_Scheduler_ queue to manage where work runs. This enables sequential and parallel composition of Futures for cleaner asynchronous code.

## Usage Synopsis

```C++
#include "Futures/Future.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::futures;

int foo(int x) {
  // do something with x
  LOG_DEVEL << "foo(" << x << ")";
  return x + 1;
}

LOG_DEVEL << "making Promise" << endl;
Promise<int> p;
Future<int> f = p.getFuture();
auto f2 = std::move(f).thenValue(&foo);
LOG_DEVEL << "Future chain made";

// ... now perhaps in another event callback

LOG_DEVEL << "fulfilling Promise";
p.setValue(42);
// .get() waits for the Future to be fulfilled
LOG_DEVEL << "Promise fulfilled f2 contains" << f2.get();
```

This would print:

```
making Promise
Future chain made
fulfilling Promise
foo(42)
Promise fulfilled f2 contains 43
```

## Guide

This brief guide covers the basics. For a more in-depth coverage skip to the appropriate section.

Let's begin with an example using our well known `transaction::Methods` interface:

```C++
class Methods {
  // ...
  public:
  OperationResult document(
    std::string const& collectionName, VPackSlice const value,
    OperationOptions& options) {...}
  // ...
};
```

This API is synchronous, i.e. when you call `document()` you have to wait for the result. This is very simple, but unfortunately it is very easy to write very slow code, hogging the available threads until a network request is answered.

Now, consider a callback based async signature for the same operation:

```C++
void document(std::string const& collectionName, VPackSlice const value,
              OperationOptions& options,
              std::function<void(OperationResult)> callback);
```

When you call `async_get()`, your asynchronous operation begins and when it finishes the callback will be called with the result. 
Very performant code can be written with an API like this, but for nontrivial applications the code devolves
 into a special kind of spaghetti code affectionately referred to as _"callback hell"_.

The Future-based API looks like this:

```C++
Future<OperationResult> document(std::string const& collectionName,
                                 VPackSlice const value, OperationOptions& options);
```
The _Future<OperationResult>_ is a placeholder for the OperationResult we might get eventually.
A Future usually starts life out "unfulfilled", or incomplete, i.e.:

```C++
fut.isReady() == false
fut.get()  // will throw an exception because the Future is not ready
```


At some point in the future, the `Future` will have been fulfilled, and we can access its value.

```C++
fut.isReady() == true
OperationResult& result = fut.get();
```
Futures support exceptions. If the asynchronous producer fails with an exception, your Future may represent an exception instead of a value. In that case:

```C++
fut.isReady() == true
fut.get() // will rethrow the exception
```

Just what is exceptional depends on the API, the important thing is that exceptional conditions (including and especially spurious exceptions that nobody expects) get captured and can be handled higher up the "stack".

## Then Handling

There are three variants of _then_: `Future::then`, `Future::thenValue`, `Future::thenError`.

The pure _then_ method will receive an argument of type `Try<T>`, the Try can contain either a value
or an exception. This way you can handle success or failure in the same lambda:

```C++
Promise<int> p;
Future<int> f = p.getFuture();
auto f2 = std::move(f).then([&](Try<int>&& t){
  if (t.hasException()) {
    // handle
  } else {
    // do something
  }
});
LOG_DEVEL << "fulfilling Promise";
p.setException(std::logic_error("abc"));
```

Alternatively you can chain `thenValue` and `thenError` to handler exceptions. The error
handler will be skipped if no exception occurred.


```C++
Promise<int> p;
Future<int> f = p.getFuture();
auto f2 = std::move(f)
.thenValue([&](int i) {
  LOG_DEVEL << "got " << i;
  throw std::logic_error("some error"); // will propagate to matching thenError
})
.thenValue([&](int i) { 
  // will be skipped due to the exception
})
.thenError<std::logic_error&>([&](std::logic_error& t){
  // handle
  return 0;
});
p.setValue(1);
```

## Aggregate Futures

TODO

## Pro / Contra Arguments

**Pro**
- Easy to integrate future pattern with existing callback based code (i.e. ClusterComm)
- Fairly simple to use with synchronous code, just call `future.get()` to wait for the async operation
- Support for exceptions is natively built in

**Contra**
- Uses an extra heap allocation per Promise / Future pair
- Timeouts are not natively handled
- Our implementation does not adhere to the C++ standard, might become a legacy code
- ??
