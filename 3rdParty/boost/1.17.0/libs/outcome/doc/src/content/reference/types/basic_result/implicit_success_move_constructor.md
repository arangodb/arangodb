+++
title = "`basic_result(success_type<T> &&)`"
description = "Implicit value-from-success-type-sugar move constructor. Available if `predicate::enable_compatible_conversion<T, void, void>` is true, or `T` is `void`. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "tagged-constructors"]
weight = 510
+++

Implicit value-from-success-type-sugar move constructor used to disambiguate the construction of the value type.
Note that if `T = void`, `value_type` will be default constructed.  Calls {{% api "void hook_result_move_construction(T *, U &&) noexcept" %}} with `this` and `success_type<T> &&`.

*Requires*: `predicate::enable_compatible_conversion<T, void, void>` is true, or `T` is `void`.

*Complexity*: Same as for the `value_type` constructor which accepts `T`, or the `value_type` default constructor if `T` is `void`. Constexpr, triviality and noexcept of underlying operations is propagated.