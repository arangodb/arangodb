+++
title = "`basic_result(A1 &&, A2 &&, Args ...)`"
description = "Implicit inplace value or error constructor. Available if `predicate::enable_inplace_value_error_constructor<A1, A2, Args ...>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "inplace-constructors"]
weight = 440
+++

Implicit inplace value or error constructor. Delegates to an appropriate explicit inplace constructor depending on input.

*Requires*: predicate::enable_inplace_value_error_constructor<A1, A2, Args ...>` is true.

*Complexity*: Same as for the `value_type` or `error_type` constructor which accepts `A1, A2, Args ...`. Constexpr, triviality and noexcept of underlying operations is propagated.

*Guarantees*: If an exception is thrown during the operation, the state of the Args is left indeterminate.
