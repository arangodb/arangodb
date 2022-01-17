+++
title = "`basic_outcome(S &&, P &&)`"
description = "Implicit `error_type` + `exception_type` constructor. Available if `predicate::enable_error_exception_converting_constructor<S, P>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "converting-constructors"]
weight = 202
+++

Implicit `error_type` + `exception_type` constructor. Calls {{% api "void on_outcome_construction(T *, U &&, V &&) noexcept" %}} with `this`, `S` and `P`.

*Requires*: `predicate::enable_error_exception_converting_constructor<S, P>` is true.

*Complexity*: Same as for `error_type`'s and `exception_type`'s copy or move constructor. Constexpr, triviality and noexcept of underlying operations is propagated.