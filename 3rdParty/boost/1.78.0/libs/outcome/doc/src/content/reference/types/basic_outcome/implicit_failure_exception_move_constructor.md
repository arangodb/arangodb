+++
title = "`basic_outcome(failure_type<EP> &&)`"
description = "Implicit exception-from-failure-type-sugar move constructor. Available if `predicate::enable_compatible_conversion<void, void, EP, void>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "tagged-constructors"]
weight = 535
+++

Implicit exception-from-failure-type-sugar move constructor used to disambiguate the construction of the exception type.
Calls {{% api "void on_outcome_move_construction(T *, U &&) noexcept" %}} with `this` and `failure_type<EP> &&`.

*Requires*: `predicate::enable_compatible_conversion<void, void, EP, void>` is true.

*Complexity*: Same as for the `exception_type` constructor which accepts `EP`. Constexpr, triviality and noexcept of underlying operations is propagated.