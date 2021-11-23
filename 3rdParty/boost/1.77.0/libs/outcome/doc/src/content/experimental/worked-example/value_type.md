+++
title = "The payload"
weight = 20
+++

We define the code domain's `value_type` -- the payload to be transported by
status codes using this code domain -- to be a POSIX `errno` value, an integer
line number and a const char pointer.

{{% snippet "experimental_status_code.cpp" "value_type" %}}

You will note that this is a `TriviallyCopyable` type, and so gains an implicit
conversion to any `status_code<erased<T>>` where `sizeof(T) >= sizeof(value_type)`.

`error` is however `status_code<erased<intptr_t>>`, and `sizeof(intptr_t) < sizeof(value_type)`,
so it is not possible to implicitly convert status codes from this domain into
`error`. Instead, you must tell the compiler how to do the conversion, as we
shall see later.
