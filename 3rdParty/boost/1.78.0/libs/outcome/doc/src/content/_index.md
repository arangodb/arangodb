+++
title = "Home"
+++

# Outcome 2.2 library

{{% boost-copyright %}}

{{% notice note %}}
At the end of December 2020, Outcome v2.2 replaced v2.1 in develop branch. This is a breaking
change and all Outcome v2.1 code will need to be upgraded using [the v2.1 => v2.2 upgrade guide]({{% relref "/changelog/upgrade_v21_v22" %}}). See also
[the list of v2.2 major changes]({{% relref "/changelog/v22" %}}).
{{% /notice %}}

Outcome is a set of tools for reporting and handling function failures in contexts where *directly* using C++ exception handling is unsuitable. Such contexts include:

  - there are programs, or parts thereof, that are compiled with exceptions disabled;

  - there are parts of program that have a lot of branches depending on types of failures,
    where if-statements are cleaner than try-catch blocks;

  - there is a hard requirement that the failure path of execution should not cost more than the successful path of execution;

  - there are situations, like in the [`filesystem`](http://www.boost.org/doc/libs/release/libs/filesystem/doc/index.htm) library, where whether the failure should be handled remotely
    (using a C++ exception throw), or locally cannot be made inside the function and needs to be decided by the caller,
    and in the latter case throwing a C++ exception is not desirable for the aforementioned reasons;

  - there are parts of the program/framework that themselves implement exception handling and prefer
    to not use exceptions to propagate failure reports across threads, tasks, fibers, etc;

  - one needs to propagate exceptions through layers that do not implement exception throw safety;

  - there is an external requirement (such as a company-wide policy) that failure handling paths are explicitly indicated in the code.

  - where interoperation with C code, without having to resort to C++ exception wrapper shims, is important.

Outcome addresses failure handling through returning a special type from functions, which is able to store either a successfully computed value (or `void`), or the information about failure. Outcome also comes with a set of idioms for dealing with such types.

Particular care has been taken to ensure that Outcome has the lowest possible impact on build times,
thus making it suitable for use in the global headers of really large codebases. Storage layout is
guaranteed and is C-compatible for `result<T, E>`[^1], thus making Outcome based code long term ABI-stable.

Fully deterministic all-`noexcept` C++ Coroutine support in Outcome is particularly strong, and we
supply Outcome-optimising {{< api "eager<T>/atomic_eager<T>" >}} and {{< api "lazy<T>/atomic_lazy<T>" >}}
awaitables which work for any user type.

## Sample usage

The main workhorse in the Outcome library is `result<T>`: it represents either a successfully computed value of type `T`, or a `std::error_code`/`boost::system::error_code`[^2] representing the reason for failure. You use it in the function's return type:

{{% snippet "intro_example.cpp" "signature" %}}

It is possible to inspect the state manually:

{{% snippet "intro_example.cpp" "inspect" %}}

Or, if this function is called in another function that also returns `result<T>`, you can use a dedicated control statement:

{{% snippet "intro_example.cpp" "implementation" %}}

`BOOST_OUTCOME_TRY` is a control statement. If the returned `result<T>` object contains an error information, the enclosing function is immediately returned with `result<U>` containing the same failure information; otherwise an automatic object of type `T`
is available in scope.

{{% notice note %}}
This library joined [the Boost C++ libraries](https://www.boost.org/doc/libs/develop/libs/outcome/doc/html/index.html) in the 1.70 release (Spring 2019). [It can be grafted into much older Boost releases if desired](https://github.com/boostorg/outcome).
{{% /notice %}}

[^1]: If you choose a C-compatible `T` and `E` type.

[^2]: `result<T>` defaults to `std::error_code` for Standalone Outcome, and to `boost::system::error_code` for Boost.Outcome. You can mandate a choice using `std_result<T>` or `boost_result<T>`.
