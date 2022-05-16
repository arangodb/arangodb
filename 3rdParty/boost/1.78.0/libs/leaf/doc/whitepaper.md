# Error Handling Principles and Practices

## Abstract

Error handling is an important part of language design and programming in general. We deconstruct the problem domain in an attempt to derive an objective understanding of the pros and cons of the various approaches to error handling. While we use C++ throughout this paper, our reasoning applies to any statically-typed language.

We present, with implementation, a novel method for communication of error objects of arbitrary types safely, without using dynamic memory allocation. In addition, we analyze the effect of aggressive inlining on the overhead of exception handling in C++, without a need to change the standard or the ABIs.

## 1. The semantics of a failure

Programmers have an intuitive understanding of the nature of error handling: while coding, we naturally reason in terms of "what if something goes wrong". One way to express this reasoning is to return from functions which may fail some `expected<T, E>` variant type, which holds a `T` in case of success or an `E` in case of failure. For example:

```c++
expected<int, error_code> compute_value();
```

Above, `compute_value` produces an `int`, but if that fails, it would communicate an object of type `error_code`, which would indicate the reason for the failure.

But here is an interesting question: what happens if the type we pass for `T` to `expected<T, E>` is itself a variant type? Of course this isn't a problem:

```c++
expected<variant<int,float>, error_code> compute_value(); // (A)
```

What makes the above function interesting is that syntactically, it returns an `int`, or a `float`, or an `error_code`; and since we're already returning a variant, could we ditch `expected<T, E>` altogether? We most definitely could:

```c++
variant<int, float, error_code> compute_value(); // (B)
```

Are the two functions (A) and (B) semantically equivalent? It appears that any algorithm working with the result from (A) can be transformed to handle a call to (B) without any loss of functionality. This follows directly form the fact that either way there are three distinct outcomes, so the differences should be purely mechanical rather than in logic.

In fact there is one subtle but critical difference: (A) encodes explicitly in the return object an additional bit of information -- "success-or-error" -- while in (B) that knowledge is implicit: we have to keep it in mind as we write the caller function, but it is not reflected in the program state.

> **Definition:** The one bit of information that indicates whether a function has failed is called the *failure flag*.

Therefore it is not true that any algorithm working with the result from (A) can be transformed to handle a call to (B): the exception is algorithms that respond generically to any error, based solely on observing the *failure flag* being set. The ability to implement such algorithms is the reason why explicitly encoding this extra bit of information in (A) is not pure overhead, compared to (B).

Of course, this reasoning applies in general: if a function reports a failure, it is important for the caller to be able to react to it without further semantic understanding of each possible error condition; that is, it is a good idea to optimize error handlig for this relatively common use case.

## 2. Classification of functions based on their affinity to errors

> **Definition:** Functions that create error objects and initiate error handling are called *error initiating* functions.

These are functions which, based on the semantics of the operations they perform, flag certain outcomes as failures, reporting the causes to their caller.

> **Definition:** Functions which forward to the caller error information communicated by lower-level functions are called *error neutral* functions.

These functions react solely based on the [*failure flag*](#1-the-semantics-of-a-failure) indicated by a lower-level function, forwarding failures to the caller without understanding the semantics of the error ("something went wrong").

For example, an *error initiating* function may initialize a new `std::error_code` object to describe a failure, while an *error neutral* function may just forward the `std::error_code` returned by a lower-level function to its own caller.

Of course, *error neutral* functions may contribute additional error information. The key difference between *error initiating* and *error neutral* functions is that the latter do not understand the semantics of lower-level failures, they simply react based on the value of the *failure flag*.

> **Definition:**
> Functions which dispose of error objects they have received and recover normal program operation are called *error handling* functions.

All information communicated by *error initiating* and *error neutral* functions in case of a failure targets an *error handling* function found up the call stack. In order to take appropriate action to resume normal program operation, that function must understand the semantics of at least some possible error conditions that may occur in lower-level functions (the exception is functions that act as catch-all handlers, which are designed to recover from any and all error conditions, possibly after logging all available relevant information.)

Note that an *error handling* function may act as *error neutral* as well, by forwarding to its caller some failures it does not recognize and therefore can not recover from.

## 3. Types of error information based on context

Consider a function which opens a file using [`open()`](http://man7.org/linux/man-pages/man2/open.2.html), reads it then parses it. If it fails, we need to know what failed:

* Did the file fail to open?
* Was the file opened successfully but the attempt to read it failed?
* Was there a parsing error after the file was successfully read?

Such simple booleans may be sufficient sometimes, but usually they are not. Reasonably, we need to respond differently depending on whether the call to `open()` resulted in `ENOENT` vs. `EACCESS` -- and if we got `EEXIST`, that would be a logic error. Therefore we need to know the relevant `errno` also.

Secondly, if the file failed to open, we need to know the `pathname` passed to `open()`, in case it happens to be incorrect. But what if it is correct and yet `open()` failed? Right, we probably need to know the `flags` argument passed to `open()` as well.

Similar reasoning applies to the reading step, except there is an additional complication: in the function that reads the file we use the `int` handle to identify the file, and therefore we don't have access to the `pathname` to report in case the call to [`read()`](http://man7.org/linux/man-pages/man2/read.2.html) fails. And if the failure is detected in the parsing step, the file may be closed already, so we may not even have the handle available.

Is it important to know the name of the file which we failed to read or parse? It sure is. What got us on the error handling path (*error neutral* functions forwarding the failure to their caller, until an *error handling* scope is reached) may be a failure to read or parse the file, but once we reach a scope where the `pathname` is available, we should be able to report it in addition to the initial error information.

## 4. Handling of error information

In the previous section we weren't concerned with how the error information (`errno`, `pathname`, etc.) is handled, we were only reasoning about what we need to know in case a failure occurs. But the question remains: yes, the `pathname` and `flags` passed to `open()` are relevant to any failure in any operation related to that file, but what are we supposed to do with them?

There are several options:

* Don't bother with any error information beyond a basic error code.
* Print the information in a log.
* Convert relevant error information to string and store that in the object returned in case of failure.
* Store them, in a type-safe manner, in the object returned in case of failure.

We'll examine each option in more detail below.

### 4.1. Communicating an error code and nothing more

This approach is actually perfect for low-level libraries. The burden of transporting additional necessary information is shifted to the caller, which is arguably where it should be in this case: all relevant information leading to failures detected in a low-level function is available in the calling scopes. For example, it would be inappropriate if a function like `read()` tries to log anything or return error information beyond an error code, because all other relevant information is available in the various scopes leading to the failure.

However, this approach does not compose: as the error code bubbles up, each scope may hold additional relevant information that needs to be communicated or lost forever as that scope is exited; and that can't be done with a simple error code. For example, it may be difficult to deal with `ENOENT` without knowing the relevant `pathname` -- and if we're aborting a scope where that information is available, we should communicate it at that point.

### 4.2. Communicating an error code, logging other relevant information

This approach offers the simplicity of communicating just an error code, but it is usable in higher-level libraries. If a low-level function reports a failure, we log any relevant information we have, then return an error code to the caller, and the process repeats until an *error handling* function is reached. Alternatively, the logging may be done as a matter of routine, not only in case a failure is detected.

There are several problems with this approach:

* It couples us with a logging system, which may not be a problem in the domain of a particular project, but certainly is not ideal for a universal library.
* Depending on the use case, it may be too costly to hit the file system or some other logging target while handling errors.
* The information in the log is developer-friendly, but not user-friendly. There are many projects where this is not a problem (for example, internet servers), but generally it is not appropriate to present a wall of cryptic diagnostic information to a user in hopes he will find clues in it to help fix the problem. But even a developer-facing command line utility has to print error information in plain English -- and the typical user-friendly app has to be able to use different languages.

### 4.3. Communicating an error code + a string

The main appeal of this approach is that like logging error information, it composes nicely: if a lower level function communicates an error code, we can easily convert it to string, together with any other information available in our scope (e.g. the `pathname`), then return the string plus an error code to the caller. And of course this can be repeated in each calling scope: concatenate relevant information to the string and pass it on together with an(other) error code.

What's more, in C++ the conversion of any type of error information to string is easy to define in terms of `std::ostream` `operator<<` overloads, so at each level we can process lower-level failures generically. In the end, the error handling function gets an error code (so it knows what went wrong), plus a string with relevant information that can be presented (or even logged) as "the reason" for the failure.

While this is similar to writing the relevant error information in a log, this approach is arguably better because we don't depend on a logging system; but other downsides remain:

* The result of the automatic conversion to string is not user-friendly.
* The ability to concatenate strings usually requires dynamic memory allocations, which may be too slow in some cases.
* Dynamic allocations may also fail, which is especially problematic during error handling.

### 4.4. Communicating all error information with type-safety

The error handling strategies we discussed so far work well as long as automatically-printed (or logged) error information is sufficient. And while there are many problem domains where this is true, we still need a solution for the case when error handling code has to react to failures intelligently and to understand the available error information, rather than just print it for a developer to analyze. This requires that the various error objects delivered to an *error handling* function retain their static type even as they cross library boundaries; for example, in case of errors, the `flags` argument passed to `open()` should be communicated as an `int` rather than a `std::string`.

More generally, this is known as "The Interoperability Problem". The following analysis is from Niall Douglas:<sup>[1](#reference)</sup>

>If library A uses `result<T, libraryA::failure_info>`, and library B uses `result<T, libraryB::error_info>` and so on, there becomes a problem for the application writer who is bringing in these third party dependencies and tying them together into an application. As a general rule, each third party library author will not have built in explicit interoperation support for unknown other third party libraries. The problem therefore lands with the application writer.
>
>The application writer has one of three choices:
>
>1. In the application, the form of result used is `result<T, std::variant<E1, E2, …​>>` where `E1`, `E2` … are the failure types for every third party library in use in the application. This has the advantage of preserving the original information exactly, but comes with a certain amount of use inconvenience and maybe excessive coupling between high level layers and implementation detail.
>
>2. One can translate/map the third party’s failure type into the application’s failure type at the point of the failure exiting the third party library and entering the application. One might do this, say, with a C preprocessor macro wrapping every invocation of the third party API from the application. This approach may lose the original failure detail, or mis-map under certain circumstances if the mapping between the two systems is not one-one.
>
>3. One can type erase the third party’s failure type into some application failure type, which can later be reconstituted if necessary. This is the cleanest solution with the least coupling issues and no problems with mis-mapping, but it almost certainly requires the use of `malloc` which the previous two did not.

An interesting observation is that the excessive coupling mentioned in the first point above is very similar to exception specifications. For example, if we have:

```c++
struct error_info
{
  int error_code;
  std::string file_name;
};
```

And then:

```c++
expected<int, error_info> compute_value(....); // (A)
```

This is very similar to:

```c++
int compute_value(....) throw(error_info); // (B)
```

The difference, of course, is that in (A) the compatibility with the caller is enforced statically, while C++ exception specifications were enforced dynamically ("were", because they are now removed from the language). So, in truth, (A) is equivalent not to (B), but to statically-enforced exception specifications, as they are in Java.

What is the problem with statically-enforced exception specifications? Herb Sutter explains:<sup>[2](#reference)</sup>

> The short answer is that nobody knows how to fix exception specifications in any language, because the dynamic enforcement C++ chose has only different (not greater or fewer) problems than the static enforcement Java chose. …​ When you go down the Java path, people love exception specifications until they find themselves all too often encouraged, or even forced, to add throws Exception, which immediately renders the exception specification entirely meaningless. (Example: Imagine writing a Java generic that manipulates an arbitrary type `T`).

In other words, in the presence of dynamic or static polymorphism, it is impractical to force a user-supplied function to conform to a specific set of error types. For example, at the point of a polymorphic call to a `read()` function, we can not reasonably predict or statically specify all the error objects it may need to communicate.

But what about non-generic contexts? Is the equivalent of statically-enforced exception specifications a good idea in that case? We'll discuss this important question next.

## 5. Error handling and function signatures

Designers of any programming language, but especially statically-typed languages, take care to provide for compile-time checks to ensure that function calls work as intended. For example, the compiler itself should be able to automatically detect bugs where the caller provides the wrong number or type of arguments.

In turn, the programmer would use the principle of encapsulation to logically decouple the caller of each function from the interface of lower level functions (A.K.A. implementation details). This is desirable because, assuming the interface of each function is correct, as long as *what* it does doesn't change, we can easily modify *how* it is done, with minimal disruption.

This results in tight coupling between a caller and a callee: in order to call a function, we must understand its semantics and the semantics of each of its arguments; it's preconditions and postconditions, etc. Of course, this is not a problem, in fact this coupling is the reason why the compiler is able to detect many errors before the program even runs.

On the other hand, not all objects a function needs to use should be passed through the narrow interface specified by its signature. For example, it is possible to pass a logger object as one of the arguments of each function which may need to print diagnostic information, but this is almost never done in practice; instead, the logging system is accessible globally. If we require that the logger is passed down as an argument, this would create difficulties for intermediate functions which don't need it: they would be coupled with an object they do not necessarily understand, only so they can pass it down to another function which may actually use it, but possibly just to pass it down to yet another function.

When an object must be communicated down the call stack to a function several levels removed from the one that initiates the call, it is usually desirable to decouple the signature of all intermediate functions from that object, because their interface has nothing to do with it.

The same reasoning applies to *error neutral* functions with regards to failures originating in lower-level functions. While it is possible to couple the signatures of intermediate functions with the static type of all the [error information they may need to communicate](#4-handling-of-error-information), this essentially destroys their neutrality towards failures. That's because, in order for each function to define a specific type to report all possible error objects statically, it must understand the exact semantics of all lower level error types. This turns what would have been a chain of calls to *error neutral* functions into a game of telephone, requiring each node to both understand and correctly re-encode each communicated failure.

## 6. Alternative mechanisms for transporting of error objects

So far we established that in general it is not a good idea to couple return values (or function signatures) of *error neutral* functions with the static type of all error objects they may need to communicate. In this section we'll discuss the alternative approaches.

### 6.1. `GetLastError` / `errno`

A typical classical approach is to only communicate the *failure flag* in the return value, while additional information is delivered through a separate mechanism. Here is an example from the Windows API:

```c
BOOL DeleteFileW(
  LBCWSTR lpFileName
);
```

If the function succeeds, the return value is non-zero. If the function fails, the return value is zero. In case of failure, the user can call `GetLastError()` to obtain an error code:

```c
DWORD GetLastError();
```

This approach is appealing because it frees the return value from the burden of transporting any error information beyond the *failure flag*, a single bit. For example:

```c
HANDLE CreateFileW(
  LPCWSTR               lpFileName,
  DWORD                 dwDesiredAccess,
  DWORD                 dwShareMode,
  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  DWORD                 dwCreationDisposition,
  DWORD                 dwFlagsAndAttributes,
  HANDLE                hTemplateFile
);
```

Had the above function returned an error code, it would have to take one more argument to output the `HANDLE` in case of success. Instead, the return value *is* the file handle, reserving the special `INVALID_HANDLE_VALUE` to indicate a failure, while additional error information is obtained by calling `GetLastError()`.

This same approach is used in many POSIX APIs, for example `open()`:

```c
int open(const char *pathname, int flags);
```

The function attempts to open the file, returning the file handle or the special value `-1` to indicate a failure. In this case, the caller inspects `errno` for the error code.

But there is another important benefit to this approach which is easy to overlook: it is specifically designed to facilitate the implementation of *error neutral* functions:

```c
float read_data_and_compute_value(const char *pathname)
{
  // Open the file, return INVALID_VALUE in case that fails:
  int fh = open(pathname, O_RDONLY);
  if (fh == -1)
    return INVALID_VALUE;

  // Read data, compute the value
  ....
}
```

The above function computes a `float` value based on the contents of the specified file, reserving a single `INVALID_VALUE` (possibly a NaN) to allow the caller to effectively inspect the *failure flag*.

In case of failure to `open()`, our function stays out of the way: the error code is communicated from `open()` (which sets the `errno`) *directly* to an *error handling* function up the call stack (which examines the `errno` in order to understand the failure), while the return value of each intermediate function needs only communicate a single bit of data (the *failure flag*).

The major drawback of this appoach is that the *failure flag* is not communicated uniformly, which means that *error neutral* functions can't check for errors generically (e.g. each layer needs to know about the different `INVALID_VALUE`s).

### 6.2. C++ Exceptions

In C++, the default mechanism for dealing with failures is exception handling. In this case, the check for errors is not only generic but completely automated: *by default*, if a function fails, the error will be communicated to the caller. Literally, the programmer can't forget to check the *failure flag*.

The drawback of virtually all implementations is overhead, both in terms of space and speed. Below we'll analyze the reasons for this overhead, and point out ways to alleviate them.

#### 6.2.1. Contemporary ABIs

Contemporary C++ exception handling is notorious for overhead<sup>[3](#reference)</sup>. There are two major flavors of exception handling implementations: frame-based and table-based:

* **Frame-based** implementations (e.g. x86) add to the cost of stack frames (that is, function calls), which is already not insignificant in performance-critical parts of the program. The added cost affects both the happy and the sad path: function calls become a bit more expensive even if no exception is thrown.

* **Table-based** implementations (e.g. Itanium, x64) add to the cost of stack frames but only on the sad path. In fact, the performance of the happy path is often improved. However, if an exception is thrown and stack unwinding must commence, the table-based approach leads to a reduction in speed, compared to frame-based implementations.

Table-based exception handling has been designed based on the questionable assumption that it is critical to eliminate speed overhead in programs that don't `throw`. I'm saying questionable, because in practice such programs are compiled with `-fno-exceptions`.

Ironically, the (older and less sophisticated) frame-based approach is preferable when we need to better control the cost of exception handling. The reason is that the overhead added to the happy path can be easily eliminated by inlining -- which is what we do anyway to deal with all other function-call overhead. This leaves only the cost of the sad path, which is also much more predictable, compared to the table-based approach.

#### 6.2.2. Communicating the *failure flag*

Current implementations do not communicate the *failure flag* explicitly. Instead, when throwing an exception, the compiler uses some form of automatic stack unwinding (possibly similar to `longjmp`) to reach the appropriate `catch` block, and to know which destructors to call.

A much better approach would be for functions which may throw to communicate the *failure flag* explicitly, hopefully in the registers rather than spilling it to memory. This would allow each exception-neutral function to implement the error check, as well as to call the correct destructors in case of an error, with very little overhead.

#### 6.2.3. Allocation of exception objects

Consider the following exception type:

```c++
struct my_error: std::exception {};
```

A catch statement designed to handle `my_error` exceptions:

```c++
try
{
  f();
}
catch(my_error & e)
{
  ....
}
```

If the above `catch` was only required to match objects of type `my_error`, they could be allocated on the stack, in the error handling scope, using automatic storage duration. However, `catch` must also match objects of any type that derives from `my_error`, and therefore the size of the exception object can not in general be known in this scope. For this reason, contemporary implementations allocate the memory for exception objects dynamically.

A better approach<sup>[4](#reference)</sup> is to pre-allocate a stack-based buffer of some generally sufficient static size, leaving the option to allocate very large exception objects dynamically, similarly to the way `std::string` is typically implemented using small string optimization.

#### 6.2.4. We do not need a new language (not even a new ABI)

There are several ongoing efforts to improve the performance of exception handling<sup>[5](#reference)</sup>, however most of them propose (and require) changes to the C++ language definition. Here we offer an alternative idea which does not, and will likely significantly alleviate the need for further improvements.

Consider the following program:

```c++
#include <exception>

int main()
{
  try
  {
    throw std::exception();
  }
  catch(std::exception & e)
  {
  }
  return 0;
}
```

And here is the generated code ([Godbolt](https://godbolt.org/z/_a8eg9)):

```x86asm
main:
        push    rcx
        mov     edi, 8
        call    __cxa_allocate_exception
        mov     edx, OFFSET FLAT:_ZNSt9exceptionD1Ev
        mov     esi, OFFSET FLAT:_ZTISt9exception
        mov     QWORD PTR [rax], OFFSET FLAT:_ZTVSt9exception+16
        mov     rdi, rax
        call    __cxa_throw
        mov     rdi, rax
        dec     rdx
        je      .L3
        call    _Unwind_Resume
.L3:
        call    __cxa_begin_catch
        call    __cxa_end_catch
        xor     eax, eax
        pop     rdx
        ret
```

Bluntly, this is not acceptable, since it is trivial to determine at compile time that the `throw`/`catch` pair is noop. What if the generated code is improved _only_ in this simple use case, where the `throw` and the matching `catch` are in the same stack frame? Under this condition, there is no need to invoke the ABI machinery to allocate an exception object or navigate the unwind tables; the analysis of the control flow can and should happen at compile time. We'd end up with simply

```x86asm
main:
        xor     eax, eax
        ret
```

Why is optimizing this simplest of use cases important? Because this leads to complete and total elimination of exception handling overhead whenever function inlining occurs; and in C++ the critical path is already heavily reliant on inlining, because the function call overhead is not insignificant even if exception handling is disabled.

### 6.3. Boost LEAF

Boost LEAF<sup>[6](#reference)</sup> is a universal error handling library for C++ which works with or without exception handling. It provides a low-cost alternative to transporting error objects in return values.

Using LEAF, *error handling* functions match error objects similarly to the way `catch` matches exceptions, with two important differences:

* Each handler can specify multiple objects to be matched by type, rather than only one.
* The error objects are matched dynamically, but solely based on their static type. This allows *all* error objects to be allocated on the stack, using automatic storage duration.

Whithout exception handling, this is achieved using the following syntax:

```c++
leaf::handle_all(

  // The first function passed to handle_all is akin to a try-block.
  []() -> leaf::result<T>
  {
    // Operations which may fail, returning a T in case of success.
    // If case of an error, any number of error objects of arbitrary
    // types can be associated with the returned result<T> object.
  },

  // The handler below is invoked if both an object of type my_error
  // and an object of another_type are associated with the error returned
  // by the try-block (above).
  [](my_error const & e1, another_type const & e2)
  {
    ....
  },

  // This handler is invoked if an object of type my_error is associated
  // with the error returned by the try-block.
  [](my_error const & e1)
  {
    ....
  },

  // This handler is invoked in all other cases, similarly to catch(...)
  []
  {
    ....
  }

);
```

With exception handling:

```c++
leaf::try_catch(

  // The first function passed to handle_all is akin to a try-block.
  []() -> T
  {
    // Operations which may fail, returning a T in case of success.
    // If case of an error, any number of error objects of arbitrary
    // types can be associated with the thrown exception object.
  },

  // The handler below is invoked if both an object of type my_error
  // and an object of another_type are associated with the exception
  // thrown by the try-block (above).
  [](my_error const & e1, another_type const & e2)
  {
    ....
  },

  // This handler is invoked if an object of type my_error is associated
  // with the exception thrown by the try-block.
  [](my_error const & e1)
  {
    ....
  },

  // This handler is invoked in all other cases, similarly to catch(...)
  []
  {
    ....
  }

);
```

In LEAF, error objects are allocated using automatic duration, stored in a `std::tuple` in the scope of `leaf::handle_all` (or `leaf::try_catch`). The type arguments of the `std::tuple` template are automatically deduced from the types of the arguments of the error handling lambdas. If the try-block attempts to communicate error objects of any other type, these objects are discarded, since no error handler can make any use of them.

The `leaf::result<T>` template can be used as a return value for functions that may fail to produce a `T`. It carries the [*failure flag*](#1-the-semantics-of-a-failure) and, in case it is set, an integer serial number of the failure, while actual error objects are immediately moved into the matching storage reserved in the scope of an error handling function (e.g. `handle_all`) found in the call stack.

## 7. Exception-safety vs. failure-safety

Many programmers dread C++ exception-safety<sup>[7](#reference)</sup>: the ability of C++ programs to respond correctly to a function call that results in throwing an exception. This fear is well founded, though it shouldn't be limited to throwing exceptions, but to all error handling, regardless of the underlying mechanism.

In "Exception Safety: Concepts and Techniques"<sup>[8](#reference)</sup> Bjarne Stroustrup explains:

> An operation on an object is said to be exception safe if that operation leaves the object in a valid state when the operation is terminated by throwing an exception. This valid state could be an error state requiring cleanup, but it must be well defined so that reasonable error handling code can be written for the object.

Consider the following edits:

> An operation on an object is said to be ~~exception-safe~~ failure-safe if that operation leaves the object in a valid state when the operation ~~is terminated by throwing an exception~~ fails. This valid state could be an error state requiring cleanup, but it must be well defined so that reasonable error handling code can be written for the object.

Without limiting the discussion to exception throwing, the second definition remains entirely correct. With this in mind, let's continue with Stroustrup's explanation of safety-guarantees of operations on standard library components after ~~an exception is thrown~~ a failure has occurred:

> * *Basic guarantee for all operations:* The basic invariants of the  standard library are maintained, and no resources, such as memory, are leaked.
>
> * *Strong guarantee for key operations:* In addition to providing the basic guarantee, either the operation succeeds, or has no effects. This guarantee is provided for key library operations, such as `push_back()`, single-element `insert()` on a list, and `uninitialized_copy()`
>
> * *~~Nothrow~~ No-fail guarantee for some operations:* In addition to providing the basic guarantee, some operations are guaranteed not to ~~throw an exception~~ fail. This guarantee is provided for a few simple operations, such  as `swap()` and `pop_back()`.

Does this generalization make sense? Is there a marked difference in safety-guarantees when throwing exceptions vs. any other error reporting mechanism?

The answer depends on how *error neutral* functions respond to failures. For example, Boost Outcome offers the macro `OUTCOME_TRY`, which can be invoked with two arguments, like so:

```c++
OUTCOME_TRY(i, BigInt::fromString(text));
```

The above expands to something like:

```c++
auto&& __result = BigInt::fromString(text);
if (!__result)
  return __result.as_failure();
auto&& i = __result.value();
```

The idea of `OUTCOME_TRY` is to support generic response to failures in *error neutral* functions. It relies on two things:

1. That the [*failure flag* can be observed generically](#1-the-semantics-of-a-failure), and
2. That it is safe to simply return from an [*error neutral* function](#2-classification-of-functions-based-on-their-affinity-to-errors) in case of a failure, forwarding error objects to the caller.

Logically, this behavior is equivalent to the compiler-generated code when calling a function which may throw an exception. Consequently, all reasoning applicable to object invariants when throwing exceptions applies equally when using `OUTCOME_TRY` (or the [LEAF](https://boostorg.github.io/leaf) analog, `BOOST_LEAF_AUTO`).

> **NOTE:** Lately there seems to be a debate in the C++ community whether the *basic guarantee* should be the minimum requirement for all user-defined types, that is, whether it should be required that even when operations fail, the basic object invariants are in place. Arguably this is beyond the scope of this paper, but the previous paragraph holds regardless: *safety-guarantees* are equally applicable, with or without exception handling.

## Summary

* We examined several [different approaches to error handling](#4-handling-of-error-information), as well as several mechanisms for [transporting of error objects of arbitrary static types safely](#44-communicating-all-error-information-with-type-safety).

* We demonstrated that [generally](#44-communicating-all-error-information-with-type-safety) it is not a good idea to [couple function signatures with the types of all error objects they need to communicate](#5-error-handling-and-function-signatures).

* We presented a novel method for transporting of error objects of arbitrary static types without using dynamic memory allocation, implemented by the [Boost LEAF library](#63-boost-leaf).

* We described an [alternative approach to implementing C++ exception handling](#62-c-exceptions) which would eliminate most overhead in practice.

* We showed that the three formal safety guarantees (*Basic*, *Strong*, *~~Nothrow~~ No-fail*) are useful when reasoning about object invariants, [regardless of how errors are communicated](#7-exception-safety-vs-failure-safety).

## Conclusions

* Error handling is a dynamic process, so the static type system is of limited assistance; for example, `ENOENT` is a *value* and not a *type*, and therefore the appropriate error handler has to be matched dynamically rather than statically.

* Because most of the functions in a program are *error neutral*, the ability to automatically (e.g. when using exception handling) or at least generically (e.g. `OUTCOME_TRY`, `BOOST_LEAF_AUTO`) forward errors to the caller is critical for correctness.

* The formal ~~exception-safety~~ failure-safety guarantees are applicable to *error neutral* functions responding to failures generically, even when not using exception handling.

* Exception-handling overhead can be virtually eliminated by ABI changes that require no changes in the C++ standard; in the case when exception-neutral functions are inlined, all overhead may be removed even without ABI changes.

## Reference

[1](#44-communicating-all-error-information-with-type-safety). Niall Douglas\
Incommensurate E types (Outcome library documentation)\
https://ned14.github.io/outcome/tutorial/advanced/interop/problem

[2](#44-communicating-all-error-information-with-type-safety). Herb Sutter\
Questions About Exception Specifications (Sutter's Mill)\
https://herbsutter.com/2007/01/24/questions-about-exception-specifications

[3](#621-contemporary-abis). Ben Craig\
Error speed benchmarking\
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1886r0.html

[4](#623-allocation-of-exception-objects). James Renwick, Tom Spink, Björn Franke\
Low-Cost Deterministic C++ Exceptions for Embedded Systems\
https://www.research.ed.ac.uk/portal/files/78829292/low_cost_deterministic_C_exceptions_for_embedded_systems.pdf

[5](#624-we-do-not-need-a-new-language-not-even-a-new-abi-). Herb Sutter\
Zero-overhead deterministic exceptions: Throwing values\
http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0709r1.pdf

[6](#63-leaf). Emil Dotchevski\
Lightweight Error Augmentation Framework (library documentation)\
https://boostorg.github.io/leaf

[7](#7-exception-safety-vs-failure-safety). David Abrahams\
Exception-Safety in Generic Components\
https://www.boost.org/community/exception_safety.html

[8](#7-exception-safety-vs-failure-safety). Bjarne Stroustrup\
Exception Safety: Concepts and Techniques\
http://www.stroustrup.com/except.pdf
