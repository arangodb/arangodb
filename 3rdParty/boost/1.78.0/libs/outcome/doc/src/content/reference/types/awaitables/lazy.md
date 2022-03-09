+++
title = "`lazy<T>/atomic_lazy<T>`"
description = "A lazily evaluated coroutine awaitable with Outcome customisation."
+++

This is very similar to {{% api "eager<T>" %}}, except that execution of the
`lazy<T>` returning function suspends immediately. Functions which return `lazy<T>`
are therefore suitable for tasks which you need to instantiate right now, but whose
execution will occur elsewhere e.g. in a separate kernel thread. Because of the very
common use case of using worker threads to execute the body of lazily executed
coroutines, most people will want to use `atomic_lazy<T>` instead of `lazy<T>`.

`atomic_lazy<T>` is like `lazy<T>`, except that the setting of the coroutine result
performs an atomic release, whilst the checking of whether the coroutine has finished
is an atomic acquire.

`lazy<T>` has similar semantics to `std::lazy<T>`, which is being standardised. See
https://wg21.link/P1056 *Add lazy coroutine (coroutine task) type*.

Example of use (must be called from within a coroutinised function):

```c++
lazy<int> func(int x)
{
  co_return x + 1;
}
...
// Always suspends perhaps causing other coroutines to execute, then resumes.
int r = co_await func(5);
```

`lazy<T>` has special semantics if `T` is a type capable of constructing from
an `exception_ptr` or `error_code` -- any exceptions thrown during the function's body
are sent via `T`, preferably via the error code route if {{% api "error_from_exception(" %}}`)`
successfully matches the exception throw. This means that a
{{% api "basic_result<T, E, NoValuePolicy>" %}} or {{% api "basic_outcome<T, EC, EP, NoValuePolicy>" %}} where one of its types is
is compatible will have its `.error()` or `.exception()` set.

Note that `lazy<T>` does not otherwise transport exception throws, and rethrows
any exceptions thrown within the coroutine body through the coroutine machinery.
This does not produce reliable consequences in current C++ compilers. You should
therefore wrap the coroutine body in a `try...catch` if `T` is not able to transport
exceptions on its own.

*Requires*: C++ coroutines to be available in your compiler.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::awaitables`

*Header*: `<boost/outcome/coroutine_support.hpp>`
