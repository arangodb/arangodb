+++
title = "Coroutines"
description = "Using Outcome in C++ Coroutines"
weight = 30
tags = [ "coroutines" ]
+++

[In v2.1.2]({{< relref "/changelog" >}}) Outcome published official support for using
Outcome within C++ coroutines. This page documents that support.

All standard C++ Coroutines have the following form:

```c++
// Coroutine functions MUST return an AWAITABLE type
AWAITABLE<int> function_name(Args ...)
{
  ... ordinary C++ ...
  if(!...)
  {
    co_return 5;  // CANNOT use ordinary 'return' from coroutines
  }
  ...
  // Possibly suspend this coroutine's execution until the
  // awaitable resumes execution of dependent code
  auto x = co_await expr_resulting_in_AWAITABLE;
  ...
}
```

The type `AWAITABLE<T>` is any type which publishes the Coroutine protocol telling
C++ how to suspend and resume execution of a coroutine which returns a `T`. It is out of scope of
this page to document how to do this, however note that the `eager<T>` and `lazy<T>`
types below are completely generic awaitables suitable for use in ANY code.
They **only** behave differently if `T`, the type being returned by the awaitable,
is an Outcome type e.g. `outcome::basic_result` or `outcome::basic_outcome`.
