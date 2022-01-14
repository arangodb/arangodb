+++
title = "`eager<T>/atomic_eager<T>`"
description = "An eagerly evaluated coroutine awaitable with Outcome customisation."
+++

This is very similar to {{% api "lazy<T>" %}}, except that execution of the `eager<T>`
returning function begins immediately, and if the function never suspends during the
course of its execution, no suspend-resume cycle occurs. Functions which return `eager<T>`
are therefore suitable for tasks which *may* require suspension, but will often complete
immediately.

`atomic_eager<T>` is like `eager<T>`, except that the setting of the coroutine result
performs an atomic release, whilst the checking of whether the coroutine has finished
is an atomic acquire.

Example of use (must be called from within a coroutinised function):

```c++
eager<int> func(int x)
{
  co_return x + 1;
}
...
// Executes like a non-coroutine function i.e. r is immediately set to 6.
int r = co_await func(5);
```

`eager<T>` has special semantics if `T` is a type capable of constructing from
an `exception_ptr` or `error_code` -- any exceptions thrown during the function's body
are sent via `T`, preferably via the error code route if {{% api "error_from_exception(" %}}`)`
successfully matches the exception throw. This means that a
{{% api "basic_result<T, E, NoValuePolicy>" %}} or {{% api "basic_outcome<T, EC, EP, NoValuePolicy>" %}} where one of its types is
is compatible will have its `.error()` or `.exception()` set.

Note that `eager<T>` does not otherwise transport exception throws, and rethrows
any exceptions thrown within the coroutine body through the coroutine machinery.
This does not produce reliable consequences in current C++ compilers. You should
therefore wrap the coroutine body in a `try...catch` if `T` is not able to transport
exceptions on its own.

*Requires*: C++ coroutines to be available in your compiler.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::awaitables`

*Header*: `<boost/outcome/coroutine_support.hpp>`
