+++
title = "Conventions"
description = "Why you should avoid custom `E` types in public APIs."
weight = 80
tags = [ "best-practice", "conventions", "idioms" ]
+++

You now know everything you need to get started with using Outcome
immediately.

The initial temptation for most beginners will be to use a bespoke
strongly typed enumeration on a case by case basis i.e. a "once off"
custom `E` type. This is usually due to experience in other languages
with sum types e.g. Rust, Haskell, Swift etc.

However this is C++! Not Rust, not Swift, not Haskell! I must caution you to always avoid using
custom `E` types in public APIs. The reason is that every time
library A using custom `E1` type must interface with library B
using custom `E2` type, you must map between those `E1` and `E2`
types.

This is information lossy, i.e. fidelity of failure gets lost
after multiple translations. It involves writing, and then
*maintaining*, a lot of annoying boilerplate. It leaks internal
implementation detail, and fails to separate concerns. And one
cannot use {{< api "BOOST_OUTCOME_TRYV(expr)/BOOST_OUTCOME_TRY(expr)" >}}
if there is no convertibility between `E` types.

The C++ 11 standard library, and Boost,
specifically ships `<system_error>` for the purpose of wrapping up
individual custom `E` types into a generic framework, where disparate
custom `E` types can discover and interact with one another.
That ships with every C++ compiler. 

For all these reasons, this is why `result` and `outcome` default
the `EC` type to error code. You should leave that default alone
where possible.

---

### tl;dr;

Please [plug your library into `std::error_code`]({{< relref "/motivation/plug_error_code" >}}),
or [equivalent]({{< relref "/experimental" >}}), and do not expose
custom `E` types in ANY public API. `result` and `outcome` default
`EC` to an error code for good reason.
