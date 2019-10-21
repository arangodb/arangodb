+++
title = "`basic_result(failure_type<T> &&)`"
description = "Implicit error-from-failure-type-sugar move constructor. Available if `predicate::enable_compatible_conversion<void, T, void>` is true, or `T` is `void`. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "tagged-constructors"]
weight = 530
+++

Implicit error-from-failure-type-sugar move constructor used to disambiguate the construction of the error type.
Note that if `T = void`, `error_type` will be default constructed.  Calls {{% api "void hook_result_move_construction(T *, U &&) noexcept" %}} with `this` and `failure_type<T> &&`.

*Requires*: `predicate::enable_compatible_conversion<void, T, void>` is true, or `T` is `void`.

*Complexity*: Same as for the `error_type` constructor which accepts `T`, or the `error_type` default constructor if `T` is `void`. Constexpr, triviality and noexcept of underlying operations is propagated.