+++
title = "`basic_outcome(R &&)`"
description = "Implicit `value_type` constructor. Available if `predicate::enable_value_converting_constructor<R>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "converting-constructors"]
weight = 180
+++

Implicit `value_type` constructor. Calls {{% api "void on_outcome_construction(T *, U &&) noexcept" %}} with `this` and `R`.

*Requires*: `predicate::enable_value_converting_constructor<R>` is true.

*Complexity*: Same as for `value_type`'s copy or move constructor. Constexpr, triviality and noexcept of underlying operations is propagated.