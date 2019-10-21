+++
title = "Hooking events"
description = "Intercepting useful events such as initial construction, copies and moves so you can capture backtraces, fire debug breakpoints etc."
weight = 50
tags = ["hooks"]
+++

Outcome provides multiple methods for user code to intercept various events which occur.
The deepest method is simply to inherit from `basic_result` or `basic_outcome`, and override member functions,
for which you will need to study the source code as that form of customisation is out of scope for this tutorial.

Another option is to supply a custom `NoValuePolicy` which can get you surprisingly
far into customisation ([see preceding section]({{< relref "/tutorial/essential/no-value/custom" >}})).

The final option, which this section covers, is to use the ADL discovered event hooks
which tell you when a namespace-localised `basic_outcome` or `basic_result` has been:

- Constructed
  - {{< api "void hook_result_construction(T *, U &&) noexcept" >}}
  - {{< api "void hook_outcome_construction(T *, U &&) noexcept" >}}
- In-place constructed
  - {{< api "void hook_result_in_place_construction(T *, in_place_type_t<U>, Args &&...) noexcept" >}}
  - {{< api "void hook_outcome_in_place_construction(T *, in_place_type_t<U>, Args &&...) noexcept" >}}
- Copied
  - {{< api "void hook_result_copy_construction(T *, U &&) noexcept" >}}
  - {{< api "void hook_outcome_copy_construction(T *, U &&) noexcept" >}}
- Moved
  - {{< api "void hook_result_move_construction(T *, U &&) noexcept" >}}
  - {{< api "void hook_outcome_move_construction(T *, U &&) noexcept" >}}

One criticism often levelled against library-based exception throw alternatives is that they do
not provide as rich a set of facilities as C++ exception throws. This section shows
you how to configure Outcome, using the ADL event hooks, to take a stack backtrace on
construction of an errored `result<T, error_code>`,
and if that `result<T, error_code>` should ever be converted into an `outcome<T, error_code, std::exception_ptr>`,
a custom `std::exception_ptr` will be just-in-time synthesised consisting of the `std::system_error`
for the error code, plus an expanded message string containing the stack backtrace of where
the error originally occurred.

One can see the use case for such a configuration where low-level, deterministic,
fixed latency code is built with `result`, and it dovetails into higher-level
application code built with `outcome` where execution time guarantees are not
important, and thus where a `malloc` is okay. One effectively has constructed a
"lazy indeterminism", or "just-in-time indeterminism" mechanism for handling
failure, but with all the rich information of throwing C++ exceptions.
