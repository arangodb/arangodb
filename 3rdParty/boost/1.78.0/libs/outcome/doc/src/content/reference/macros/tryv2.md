+++
title = "`BOOST_OUTCOME_TRYV2(spec, expr)`"
description = "Evaluate an expression which results in an understood type, continuing execution if successful, immediately returning `try_operation_return_as(X)` from the calling function if unsuccessful."
+++

Evaluate an expression which results in a type matching the following customisation points, continuing execution if successful, immediately returning {{% api "try_operation_return_as(X)" %}} from the calling function if unsuccessful:

- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_has_value(X)" %}}
- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_return_as(X)" %}}
- `BOOST_OUTCOME_V2_NAMESPACE::`{{% api "try_operation_extract_value(X)" %}}

Default overloads for these customisation points are provided. See [the recipe for supporting foreign input to `BOOST_OUTCOME_TRY`]({{% relref "/recipes/foreign-try" %}}).

Hints are given to the compiler that the expression will be successful. If you expect failure, you should use {{% api "BOOST_OUTCOME_TRYV2_FAILURE_LIKELY(spec, expr)" %}} instead.

*Overridable*: Not overridable.

*Definition*: Firstly the expression's temporary is bound to a uniquely named, stack allocated, `spec`. If that reference's bound object's `try_operation_has_value()` is false, immediately execute `return try_operation_return_as(propagated unique reference);`, propagating the rvalue/lvalue/etc-ness of the original expression.

*Header*: `<boost/outcome/try.hpp>`
