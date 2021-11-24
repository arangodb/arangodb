+++
title = "Hooking events"
description = "Intercepting useful events such as initial construction, copies and moves so you can capture backtraces, fire debug breakpoints etc."
weight = 50
tags = ["hooks"]
+++

Outcome provides multiple methods for user code to intercept various lifecycle events which occur.
The deepest method is simply to inherit from `basic_result` or `basic_outcome`, and override member functions,
for which you will need to study the source code as that form of customisation is out of scope for this tutorial.

Another option is to supply a custom `NoValuePolicy`
([see preceding section]({{< relref "/tutorial/essential/no-value/custom" >}})).
From Outcome v2.2 onwards, intercepting construction, copies and moves *requires*
a custom `NoValuePolicy`.

Before Outcome v2.2, there was an ADL discovered event hook mechanism for intercepting
construction, copies and moves (it was found to be brittle, error prone and surprising
in empirical use, which is why it was replaced). The ADL discovered event hooks still
function in Outcome v2.2 and later if {{% api "BOOST_OUTCOME_ENABLE_LEGACY_SUPPORT_FOR" %}}
is set to less than `220` to enable emulation.

You will note that the naming is simply one of `hook_*` => `on_*`, the parameters remain
identical. This eases porting code from Outcome v2.1 to v2.2, it's usually just a case
of copy-pasting the ADL hook code into a custom `NoValuePolicy`.

--- 
**Policy set event hooks (Outcome v2.2 onwards):**

- Constructed
  - {{< api "void on_result_construction(T *, U &&) noexcept" >}}
  - {{< api "void on_outcome_construction(T *, U &&) noexcept" >}}
- In-place constructed
  - {{< api "void on_result_in_place_construction(T *, in_place_type_t<U>, Args &&...) noexcept" >}}
  - {{< api "void on_outcome_in_place_construction(T *, in_place_type_t<U>, Args &&...) noexcept" >}}
- Copied
  - {{< api "void on_result_copy_construction(T *, U &&) noexcept" >}}
  - {{< api "void on_outcome_copy_construction(T *, U &&) noexcept" >}}
- Moved
  - {{< api "void on_result_move_construction(T *, U &&) noexcept" >}}
  - {{< api "void on_outcome_move_construction(T *, U &&) noexcept" >}}

---
**ADL discovered event hooks (before Outcome v2.2):**

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

---

One criticism often levelled against library-based exception throw alternatives is that they do
not provide as rich a set of facilities as C++ exception throws. This section shows
you how to configure Outcome, using the event hooks, to take a stack backtrace on
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
