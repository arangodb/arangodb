+++
title = "`BOOST_OUTCOME_CO_TRY_FAILURE_LIKELY(var, expr)`"
description = "Evaluate within a coroutine an expression which results in an understood type, assigning `T` to a decl called `var` if successful, immediately returning `try_operation_return_as(X)` from the calling function if unsuccessful."
+++

Evaluate within a coroutine an expression which results in a type matching the following customisation points, assigning `T` to a decl called `var` if successful, immediately returning {{% api "try_operation_return_as(X)" %}} from the calling function if unsuccessful:

- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_has_value(X)" %}}
- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_return_as(X)" %}}
- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_extract_value(X)" %}}

Default overloads for these customisation points are provided. See [the recipe for supporting foreign input to `BOOST_OUTCOME_TRY`]({{% relref "/recipes/foreign-try" %}}).

Hints are given to the compiler that the expression will be unsuccessful. If you expect success, you should use {{% api "BOOST_OUTCOME_CO_TRY(var, expr)" %}} instead.

An internal temporary to hold the value of the expression is created, which generally invokes a copy/move. [If you wish to never copy/move, you can tell this macro to create the internal temporary as a reference instead.]({{% relref "/tutorial/essential/result/try_ref" %}})

*Overridable*: Not overridable.

*Definition*: See {{% api "BOOST_OUTCOME_CO_TRYV(expr)" %}} for most of the mechanics.

If successful, `var` is initialised or assigned to the expression result's `.assume_value()` if available, else to its `.value()`. This binds a reference possibly to the `T` stored inside the bound result of the expression, but possibly also to a temporary emitted from the value observer function.

*Header*: `<boost/outcome/try.hpp>`

*Legacy*: Before Outcome v2.2, `var` was always declared as an automatic rvalue ref. You can use the backwards compatibility macro `OUTCOME21_CO_TRY()` if wish to retain the old behaviour.
