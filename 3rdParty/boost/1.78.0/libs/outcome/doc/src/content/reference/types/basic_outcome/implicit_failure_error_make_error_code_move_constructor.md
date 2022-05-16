+++
title = "`basic_outcome(failure_type<EC> &&)`"
description = "Implicit error-from-failure-type-sugar move constructor. Available if `predicate::enable_make_error_code_compatible_conversion<void, EC, void, void>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "tagged-constructors"]
weight = 532
+++

Implicit error-from-failure-type-sugar move constructor used to disambiguate the construction of the error type.
Calls {{% api "void on_outcome_move_construction(T *, U &&) noexcept" %}} with `this` and `failure_type<EC> &&`.

*Requires*: `predicate::enable_make_error_code_compatible_conversion<void, EC, void, void>` is true.

*Complexity*: Same as for the `error_type` constructor which accepts `EC`. Constexpr, triviality and noexcept of underlying operations is propagated.