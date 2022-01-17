+++
title = "Inspecting outcome<T, EC, EP>"
description = ""
weight = 30
tags = ["outcome", "value", "error", "exception"]
+++

Continuing with the previous example, in `Layer3` we have function `z` which again reports failures via exceptions.
It will call function `h` from `Layer2_old` which returns `outcome<int>` (which may store an `int` or a `std::error_code` or a `std::exception_ptr`).
The goal is to unpack it to either the successful return value `int` or to throw an appropriate exception: if we are storing an `std::exception_ptr`, just rethrow it.
If we are storing a `std::error_code` throw it as `std::system_error`, which is designed to store `std::error_code`'s:

{{% snippet "using_outcome.cpp" "def_z" %}}

Function `has_exception()` checks if it is `EP` (`std::exception_ptr`) that is stored, function `exception()` accesses it. Similarly, function `error()` accesses the `EC` (`std::error_code`) if it is stored.
`outcome<>` also has a function `has_failure()` to check if either `EC` or `EP` is being stored.

It would seem obvious that the above pattern of 'unpacking' `outcome<>`
is so common that it ought to be implemented inside function `.value()`,
so function `z` could be written as:

```c++
return old::h().value();
```

And this is exactly what the aforementioned no-value policy template argument is for, which is described in
the next section.