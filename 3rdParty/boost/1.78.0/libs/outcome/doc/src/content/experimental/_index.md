+++
title = "Experimental"
weight = 15
+++

In `<boost/outcome/experimental>`, there ships an Outcome-based simulation of
the proposed [P1095 *Zero overhead deterministic failure*](https://wg21.link/P1095)
specific implementation of [P0709 *Zero overhead exceptions: Throwing values*](http://wg21.link/P0709).
This library-only implementation lets you use a close simulacrum
of potential future C++ lightweight exceptions today in [any C++ 14 compiler
which Outcome supports]({{< relref "/requirements" >}}).

This Experimental Outcome implementation has been in production use for some
years now. It has shipped on at least one billion devices, as part of a
games suite very popular on Microsoft Windows, Apple iOS and Google Android
devices. It powers big iron enterprise applications as well, indeed all
trades including futures in the United States are captured live into a database
for the SEC by an Experimental Outcome based codebase. Finally, Experimental
Outcome is used in the firmware of parts of driver assisting cars where its
particularly rich and flexible failure added information combined with
compatibility with globally disabled C++ exceptions proved to be a big win.

The base for failure handling in future C++ might be `std::error` from [P1028
`status_code`](https://wg21.link/P1028). This proposal is currently being
refined before WG21's Library Evolution Working Group with the expectation that
it will be standardised as a large enhancement and backwards compatible superset
of `std::error_code` which is also capable of transporting any move-only type
such as `std::exception_ptr`. Like `std::error_code`, proposed `std::error`
occupies exactly two CPU registers of layout, and thus is extremely lightweight.
It can wrap arbitrary third party error handling systems, and automatically
constructs from `std::error_code` and `std::exception_ptr`, propagating the
original underlying implementations (no matter how customised) exactly (e.g.
`boost::exception_ptr`). Unlike `<system_error>`, `std::error` does not have
dependencies on expensive standard library headers, so including it into your
global build has a very low build time impact. `std::error` knows how to throw
itself as a conventional C++ exception, and knows how to losslessly consume
arbitrary C++ exception throws. Finally, `std::error` is ABI stable, and is
a [P1029 move bitcopying](https://wg21.link/P1029) type whereby moved-from
objects do not need to be destroyed.

---

Experimental Outcome uses the [same proposed `std::error` object as P1095 would do
for its `E` type](https://wg21.link/P1028) by bundling internally a copy of
https://ned14.github.io/status-code/, the reference implementation library
for proposed `std::error`. Outcome emulates move bitcopying semantics for types
declaring themselves move bitcopying via the trait {{% api "is_move_bitcopying<T>" %}},
and status codes opt into this. This greatly improves codegen to be no worse
than with `std::error_code` (a trivially copyable type), as https://godbolt.org/z/GEdEGc
demonstrates, despite that proposed `std::error` is a move-only type with
a non-trivial destructor.

Outcome binds `status_code` into `basic_result` and `basic_outcome` customisations
via the following headers:

- `<boost/outcome/experimental/status_result.hpp>`
- `<boost/outcome/experimental/status_outcome.hpp>`

These headers import the entire contents of the `BOOST_OUTCOME_SYSTEM_ERROR2_NAMESPACE`
namespace into the `BOOST_OUTCOME_V2_NAMESPACE::experimental` namespace. You
can thus address everything in `BOOST_OUTCOME_SYSTEM_ERROR2_NAMESPACE` via
`BOOST_OUTCOME_V2_NAMESPACE::experimental`.

As P1095 also proposes C language support for lightweight C++ exceptions,
experimental Outcome also has a macro-based C interface that enables C
code to work with the C-compatible subset of `status_result<T, E>`:

- `<boost/outcome/experimental/result.h>`

For non-Windows non-POSIX platforms such as some embedded systems, standalone
Experimental Outcome can be used with the `BOOST_OUTCOME_SYSTEM_ERROR2_NOT_POSIX` macro
defined. This does not include POSIX headers, and makes available a high fidelity,
fully deterministic, alternative to C++ exceptions on such platforms.

Finally, there is a single include edition of Experimental Outcome, which
can be found at https://github.com/ned14/outcome/blob/develop/single-header/outcome-experimental.hpp.
You can play with this on godbolt by `#include <outcome-experimental.hpp>`.

{{% notice warning %}}
<b>It is stressed, in the strongest possible terms, that any item inside
`<boost/outcome/experimental>` is subject to unannounced breaking change based
on WG21 standards committee feedback</b>. That said, the chances are high
that most of those breaking changes will be to naming rather than to
fundamental semantics, so you can upgrade with a bit of find and replace.
{{% /notice %}}

