+++
title = "`BOOST_OUTCOME_TRY(var, expr)`"
description = "Evaluate an expression which results in an understood type, assigning `T` to a variable called `var` if successful, immediately returning `try_operation_return_as(X)` from the calling function if unsuccessful."
+++

Evaluate an expression which results in a type matching the following customisation points, assigning `T` to a variable called `var` if successful, immediately returning {{% api "try_operation_return_as(X)" %}} from the calling function if unsuccessful:

- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_has_value(X)" %}}
- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_return_as(X)" %}}
- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_extract_value(X)" %}}

Default overloads for these customisation points are provided. See [the recipe for supporting foreign input to `BOOST_OUTCOME_TRY`]({{% relref "/recipes/foreign-try" %}}).

*Overridable*: Not overridable.

*Definition*: See {{% api "BOOST_OUTCOME_TRYV(expr)" %}} for most of the mechanics.

If successful, an `auto &&var` is initialised to the expression result's `.assume_value()` if available, else to its `.value()`. This binds a reference possibly to the `T` stored inside the bound result of the expression, but possibly also to a temporary emitted from the value observer function.

*Header*: `<boost/outcome/try.hpp>`

