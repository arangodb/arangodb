+++
title = "`explicit basic_result(basic_result<R, S, P> &&)`"
description = "Explicit converting move constructor from compatible `basic_result`. Available if `predicate::enable_compatible_conversion<R, S, P>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "explicit-constructors", "converting-constructors"]
weight = 320
+++

Explicit converting move constructor from compatible `basic_result`. Calls {{% api "void on_result_move_construction(T *, U &&) noexcept" %}} with `this` and the input.

*Requires*: `predicate::enable_compatible_conversion<R, S, P>` is true.

*Complexity*: Same as for the move constructors of the underlying types. Constexpr, triviality and noexcept of underlying operations is propagated.

*Guarantees*: If an exception is thrown during the operation, the object is left in a partially completed state, as per the normal rules for the same operation on a `struct`.
