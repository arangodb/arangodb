+++
title = "`basic_outcome(P &&)`"
description = "Implicit `exception_type` constructor. Available if `predicate::enable_exception_converting_constructor<P>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "converting-constructors"]
weight = 201
+++

Implicit `exception_type` constructor. Calls {{% api "void on_outcome_construction(T *, U &&) noexcept" %}} with `this` and `P`.

*Requires*: `predicate::enable_exception_converting_constructor<P>` is true.

*Complexity*: Same as for `exception_type`'s copy or move constructor. Constexpr, triviality and noexcept of underlying operations is propagated.