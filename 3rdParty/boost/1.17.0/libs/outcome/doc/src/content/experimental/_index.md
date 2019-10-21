+++
title = "Experimental"
weight = 15
+++

In `<boost/outcome/experimental>`, there ships an Outcome-based simulation of
the proposed [P1095 *Zero overhead deterministic failure*](https://wg21.link/P1095)
specific implementation of [P0709 *Zero overhead exceptions: Throwing values*](http://wg21.link/P0709), aka "Herbceptions". This library-only implementation lets you use a close simulacrum
of potential future C++ lightweight exceptions today in [any C++ 14 compiler
which Outcome supports]({{< relref "/requirements" >}}).

{{% notice warning %}}
<b>It is stressed, in the strongest possible terms, that any item inside
`<boost/outcome/experimental>` is subject to unannounced breaking change based
on WG21 standards committee feedback</b>. That said, the chances are high
that most of those breaking changes will be to naming rather than to
fundamental semantics, so you can upgrade with a bit of find and replace.
There are quite a few large code bases out there
already using this experimental support in anger, we know it works well
at scale and it's a good bit superior to `std::error_code` et al on every
measure.
{{% /notice %}}

P1095's support library has a reference implementation at https://ned14.github.io/status-code/.
You will find terse documentation there, and an API reference.
This library is wholly incorporated into Outcome in the `<boost/outcome/experimental/status-code>`
directory, with bindings into Outcome provided in the following headers:

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
