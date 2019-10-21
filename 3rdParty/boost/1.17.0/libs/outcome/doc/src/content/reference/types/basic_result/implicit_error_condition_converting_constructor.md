+++
title = "`basic_result(ErrorCondEnum &&)`"
description = "Implicit `error_type` from `ErrorCondEnum` constructor. Available if `predicate::enable_error_condition_converting_constructor<ErrorCondEnum>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "converting-constructors"]
weight = 200
+++

Implicit `error_type` from `ErrorCondEnum` constructor. Calls {{% api "void hook_result_construction(T *, U &&) noexcept" %}} with `this` and `ErrorCondEnum`.

*Requires*: `predicate::enable_error_condition_converting_constructor<R>` is true.

*Complexity*: Same as for `error_type`'s copy or move constructor from the result of `make_error_code(ErrorCondEnum)`. Constexpr, triviality and noexcept of underlying operations is propagated.

*Guarantees*: If an exception is thrown during the operation, the state of the input is left indeterminate.
