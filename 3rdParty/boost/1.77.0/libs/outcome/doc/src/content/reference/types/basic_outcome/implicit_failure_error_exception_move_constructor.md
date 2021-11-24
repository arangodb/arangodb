+++
title = "`basic_outcome(failure_type<EC, EP> &&)`"
description = "Implicit error-and-exception-from-failure-type-sugar move constructor. Available if `predicate::enable_compatible_conversion<void, EC, EP, void>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "tagged-constructors"]
weight = 537
+++

Implicit error-and-exception-from-failure-type-sugar move constructor used to disambiguate the construction of the error + exception types.
Calls {{% api "void on_outcome_move_construction(T *, U &&, V &&) noexcept" %}} with `this`, `failure_type<EC> &&` and `failure_type<EP> &&`.

*Requires*: `predicate::enable_compatible_conversion<void, EC, EP, void>` is true.

*Complexity*: Same as for the `error_type` and `exception_type` constructors which accept `EC` and `EP`. Constexpr, triviality and noexcept of underlying operations is propagated.