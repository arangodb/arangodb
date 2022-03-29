+++
title = "ASIO/Networking TS : Boost < 1.70"
description = "How to teach ASIO/Networking TS about Outcome."
tags = [ "asio", "networking-ts" ]
+++

*Thanks to [Christos Stratopoulos](https://github.com/cstratopoulos) for this Outcome recipe.*

---

### Compatibility note

This recipe targets Boost versions before 1.70, where coroutine support is based around
the `asio::experimental::this_coro::token` completion token. For integration with Boost
versions 1.70 and onward, see [this recipe](asio-integration-1-70).


---

### Use case

[Boost.ASIO](https://www.boost.org/doc/libs/develop/doc/html/boost_asio.html)
and [standalone ASIO](https://think-async.com/Asio/) provide the
[`async_result`](https://www.boost.org/doc/libs/develop/doc/html/boost_asio/reference/async_result.html)
customisation point for adapting arbitrary third party libraries, such as Outcome, into ASIO.

Historically in ASIO you need to pass completion handler instances
to the ASIO asynchronous i/o initiation functions. These get executed when the i/o
completes.

{{% snippet "boost-only/asio_integration.cpp" "old-use-case" %}}

One of the big value adds of the Coroutines TS is the ability to not have to write
so much boilerplate if you have a Coroutines supporting compiler:

{{% snippet "boost-only/asio_integration.cpp" "new-use-case" %}}

The default ASIO implementation always throws exceptions on failure through
its coroutine token transformation. The [`redirect_error`](https://www.boost.org/doc/libs/develop/doc/html/boost_asio/reference/experimental__redirect_error.html)
token transformation recovers the option to use the `error_code` interface,
but it suffers from the [same drawbacks]({{< relref "/motivation/error_codes" >}})
that make pure error codes unappealing in the synchronous case.

This recipe fixes that by making it possible for coroutinised
i/o in ASIO to return a `result<T>`:

{{% snippet "boost-only/asio_integration.cpp" "outcome-use-case" %}}

---

### Implementation

{{% notice warning %}}
The below involves a lot of ASIO voodoo. **NO SUPPORT WILL BE GIVEN HERE FOR THE ASIO
CODE BELOW**. Please raise any questions or problems that you have with how to implement
this sort of stuff in ASIO
on [Stackoverflow #boost-asio](https://stackoverflow.com/questions/tagged/boost-asio).
{{% /notice %}}

The real world, production-level recipe can be found at the bottom of this page.
You ought to use that in any real world use case.

It is however worth providing a walkthrough of a simplified edition of the real world
recipe, as a lot of barely documented ASIO voodoo is involved. You should not
use the code presented next in your own code, it is too simplified. But it should
help you understand how the real implementation works.

Firstly we need to define some helper type sugar and a factory function for wrapping
any arbitrary third party completion token with that type sugar:

{{% snippet "boost-only/asio_integration.cpp" "as_result" %}}

Next we tell ASIO about a new completion token it ought to recognise by specialising
[`async_result`](https://www.boost.org/doc/libs/develop/doc/html/boost_asio/reference/async_result.html):

{{% snippet "boost-only/asio_integration.cpp" "async_result1" %}}

The tricky part to understand is that our `async_result` specialisation inherits
from an `async_result` for the supplied completion token type with a completion
handler which consumes a `result<T>`. Our `async_result` is actually therefore
the base `async_result`, but we layer on top a `completion_handler_type` with
the `void(error_code, size_t)` signature which constructs from that a `result`:

{{% snippet "boost-only/asio_integration.cpp" "async_result2" %}}

To use, simply wrap the third party completion token with `as_result` to cause
ASIO to return from `co_await` a `result` instead of throwing exceptions on
failure:

```c++
char buffer[1024];
asio::experimental::await_token token =
  co_await asio::experimental::this_coro::token();

outcome::result<size_t, error_code> bytesread =
  co_await skt.async_read_some(asio::buffer(buffer), as_result(token));
```

The real world production-level implementation below is a lot more complex than the
above which has been deliberately simplified to aid exposition. The above
should help you get up and running with the below, eventually.

One again I would like to remind you that Outcome is not the appropriate place
to seek help with ASIO voodoo. Please ask on
[Stackoverflow #boost-asio](https://stackoverflow.com/questions/tagged/boost-asio).

---

Here follows the real world, production-level adapation of Outcome into
ASIO, written and maintained by [Christos Stratopoulos](https://github.com/cstratopoulos).
If the following does not load due to Javascript being disabled, you can visit the gist at 
https://gist.github.com/cstratopoulos/901b5cdd41d07c6ce6d83798b09ecf9b/da584844f58353915dc2600fba959813f793b456.

{{% gist "cstratopoulos" "901b5cdd41d07c6ce6d83798b09ecf9b/da584844f58353915dc2600fba959813f793b456" %}}
