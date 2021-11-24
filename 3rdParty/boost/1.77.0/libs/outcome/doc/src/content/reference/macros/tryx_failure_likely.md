+++
title = "`BOOST_OUTCOME_TRYX_FAILURE_LIKELY(expr)`"
description = "Evaluate an expression which results in an understood type, emitting the `T` if successful, immediately returning `try_operation_return_as(X)` from the calling function if unsuccessful."
+++

Evaluate an expression which results in a type matching the following customisation points, emitting the `T` if successful, immediately returning {{% api "try_operation_return_as(X)" %}} from the calling function if unsuccessful:

- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_has_value(X)" %}}
- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_return_as(X)" %}}
- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_extract_value(X)" %}}

Default overloads for these customisation points are provided. See [the recipe for supporting foreign input to `BOOST_OUTCOME_TRY`]({{% relref "/recipes/foreign-try" %}}).

Hints are given to the compiler that the expression will be unsuccessful. If you expect success, you should use {{% api "BOOST_OUTCOME_TRYX(expr)" %}} instead.

An internal temporary to hold the value of the expression is created, which generally invokes a copy/move. [If you wish to never copy/move, you can tell this macro to create the internal temporary as a reference instead.]({{% relref "/tutorial/essential/result/try_ref" %}})

*Availability*: GCC and clang only. Use `#ifdef BOOST_OUTCOME_TRYX` to determine if available.

*Overridable*: Not overridable.

*Definition*: See {{% api "BOOST_OUTCOME_TRYV(expr)" %}} for most of the mechanics.

This macro makes use of a proprietary extension in GCC and clang to emit the `T` from a successful expression. You can thus use `BOOST_OUTCOME_TRYX(expr)` directly in expressions e.g. `auto x = y + BOOST_OUTCOME_TRYX(foo(z));`.

Be aware there are compiler quirks in preserving the rvalue/lvalue/etc-ness of emitted `T`'s, specifically copy or move constructors may be called unexpectedly and/or copy elision not work as expected. If these prove to be problematic, use {{% api "BOOST_OUTCOME_TRY(var, expr)" %}} instead.

*Header*: `<boost/outcome/try.hpp>`
