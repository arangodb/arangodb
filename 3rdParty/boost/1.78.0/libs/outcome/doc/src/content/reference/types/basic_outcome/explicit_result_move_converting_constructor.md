+++
title = "`explicit basic_outcome(basic_result<A, B, C> &&)`"
description = "Explicit converting move constructor from compatible `basic_result`. Available if `predicate::enable_compatible_conversion<A, B, void, C>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "explicit-constructors", "converting-constructors"]
weight = 340
+++

Explicit converting move constructor from compatible `basic_result`. Calls {{% api "void on_outcome_move_construction(T *, U &&) noexcept" %}} with `this` and the input.

*Requires*: `predicate::enable_compatible_conversion<A, B, void, C>` is true.

*Complexity*: Same as for the move constructors of the underlying types. Constexpr, triviality and noexcept of underlying operations is propagated.

*Guarantees*: If an exception is thrown during the operation, the object is left in a partially completed state, as per the normal rules for the same operation on a `struct`.
