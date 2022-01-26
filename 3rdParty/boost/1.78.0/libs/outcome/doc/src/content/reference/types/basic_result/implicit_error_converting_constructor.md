+++
title = "`basic_result(S &&)`"
description = "Implicit `error_type` constructor. Available if `predicate::enable_error_converting_constructor<S>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "converting-constructors"]
weight = 190
+++

Implicit `error_type` constructor. Calls {{% api "void on_result_construction(T *, U &&) noexcept" %}} with `this` and `S`.

*Requires*: `predicate::enable_error_converting_constructor<S>` is true.

*Complexity*: Same as for `error_type`'s copy or move constructor. Constexpr, triviality and noexcept of underlying operations is propagated.