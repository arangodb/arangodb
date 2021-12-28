+++
title = "Returning Outcome types from Coroutines"
weight = 30
tags = [ "coroutines" ]
+++

`eager<T>` and `lazy<T>` and their atomic editions are completely standard
awaitables with no special behaviours, **except** if `T` is a `basic_result`
or `basic_outcome`. In that situation, the following occurs:

If the Coroutine throws a C++ exception which was not handled inside the Coroutine
body, Outcome's awaitable types try to convert it into a form which your Result or
Outcome type being returned can transport. For example:

- If your Coroutine were returning a `result<T, std::exception_ptr>`, an
errored Result with a pointer to the exception thrown would be returned.

- If your Coroutine were returning a `result<T, std::error_code>`, the
exception ptr is passed to {{% api "error_from_exception(" %}}`)` to see
if it can be matched to an equivalent `std::error_code`. If it can, an
errored Result with the equivalent error code would be returned.

- If your Coroutine were returning an `outcome<T, std::error_code, std::exception_ptr>`,
an Errored Outcome is chosen preferentially to an Excepted Outcome.

- If your Coroutine were returning an `experimental::status_result<T, system_code>`,
because Experimental SG14 `system_code` can transport error codes or
exception ptrs (or indeed `std::error_code`'s), an errored Result
is returned.
