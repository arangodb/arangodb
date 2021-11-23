+++
title = "Exceptions"
description = "Exceptions with their good and bad sides."
weight = 10
+++


Exceptions are the default mechanism in C++ for reporting, propagating and
processing the information about function failures. Their main advantage is
the ability to describe the "success dependency" between functions: if you want to
say that calling function `g()` depends on the successful execution of function `f()`,
you just put `g()` below `f()` and that's it:

```c++
int a()
{
  f();
  g();        // don't call g() and further if f() fails
  return h(); // don't call h() if g() fails
}
```

In the C++ Standard terms this means that `f()` is *sequenced before* `g()`.
This makes failure handling extremely easy: in a lot of cases you do not have
to do anything.

Also, while next operations are being canceled, the exception object containing
the information about the initial failure is kept on the side. When at some point
the cancellation cascade is stopped by an exception handler, the exception object
can be inspected. It can contain arbitrarily big amount of data about the failure
reason, including the entire call stack.


### Downsides

There are two kinds of overheads caused by the exception handling mechanism. The
first is connected with storing the exceptions on the side. Stack unwinding works
independently in each thread of execution; each thread can be potentially handling
a number of exceptions (even though only one exception can be active in one thread).
This requires being prepared for storing an arbitrary number of exceptions of arbitrary
types per thread. Additional things like jump tables also need to be stored in the
program binaries.

Second overhead is experienced when throwing an exception and trying to find the
handler. Since nearly any function can throw an exception of any time, this is
a dynamic memory allocation. The type of an exception is erased and a run-time type
identification (RTTI) is required to asses the type of the active exception object.
The worst case time required for matching exceptions against handlers cannot be easily
predicted and therefore exceptions are not suitable for real-time or low-latency
systems.

Another problem connected with exceptions is that while they are good for program
flows with linear "success dependency", they become inconvenient in situations where
this success dependency does not occur. One such notable example is releasing acquired
resources which needs to be performed even if previous operations have failed.
Another example is when some function `c()` depends on the success of at least one
of two functions `a()` and `b()` (which try, for instance, to store user data by
two different means), another example is when implementing a strong exception safety
guarantee we may need to apply some fallback actions when previous operations have
failed. When failures are reported by exceptions, the semantics of canceling all
subsequent operations is a hindrance rather than help; these situations require special
and non-trivial idioms to be employed.

For these reasons in some projects using exceptions is forbidden. Compilers offer
switches to disable exceptions altogether (they refuse to compile a `throw`, and turn
already compiled `throw`s into calls to `std::abort()`).
