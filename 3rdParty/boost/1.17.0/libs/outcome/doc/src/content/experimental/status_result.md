+++
title = "`status_result` and `status_outcome`"
weight = 40
+++

`status_result` and `status_outcome` are type aliases to {{< api "basic_result<T, E, NoValuePolicy>" >}}
and {{< api "basic_outcome<T, EC, EP, NoValuePolicy>" >}} in the usual way, but
with a defaulted `NoValuePolicy` which selects on the basis of `status_code<DomainType>`
instead.

{{% notice note %}}
If the `E` type is not some `status_code<>`, the default policy selector
will complain.
{{% /notice %}}

The specifications are:

```c++
experimental::status_result<T, E = experimental::system_code>
experimental::status_outcome<T, E = experimental::system_code, EP = std::exception_ptr>
```

So, the default `E` is the erased status code `system_code`, which can represent
any `generic_code`, `posix_code`, `win32_code`, `nt_code`, `com_code` and many
other integer error and status
codings. **Note** that `system_code` may represent successes as well as failures.
This mirrors, somewhat, how `std::error_code` can have an all bits zero defaulted
state.

You can absolutely choose an `E` type which is non-erased e.g. `posix_code` directly.
You can also choose an `E` type which is contract guaranteed to be a failure
rather than an unknown success or failure -- see `errored_status_code`.

Whether to choose typed status codes versus the erased status codes depends on your
use cases. Outcome replicates faithfully the implicit and explicit conversion
semantics of its underlying types, so you can mix results and outcomes of
`<system_error2>` types exactly as you can the `<system_error2>` types themselves
e.g. typed forms will implicitly convert into erased forms if the source type
is trivially copyable or move relocating. This means that you can return a
`generic_code` from a function returning a `system_code` or `error`, and it'll
work exactly as you'd expect (implicit conversion).

{{% notice note %}}
As `status_code<erased<T>>` is move-only, so is any `status_result` or `status_outcome`.
For some reason this surprises a lot of people, and they tend to react by not using the erased
form because it seems "difficult".
{{% /notice %}}

It is actually, in fact, a wise discipline to follow to make all functions return
move-only types if you care about determinism and performance. Whilst C++ 17 onwards
does much to have the compiler avoid copying of identical function return values thanks to
guaranteed copy elision, when a chain of functions return different types, if the
programmer forgets to scatter `std::move()` appropriately, copies rather than moves
tend to occur in non-obvious ways. No doubt future C++ standards will improve on the
automatic use of moves instead of copies where possible, but until then making all
your `result` and `outcome` types move-only is an excellent discipline.

Note that move-only `result` and `outcome` capable code (i.e. your project is in Experimental
Outcome configuration) usually compiles fine when `result` and `outcome` are copyable
(i.e. your project is in Standard Outcome configuration), albeit sometimes with a few
compiler warnings about unnecessary use of `std::move()`.
