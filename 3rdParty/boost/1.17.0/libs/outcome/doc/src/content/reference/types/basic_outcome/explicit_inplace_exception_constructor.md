+++
title = "`explicit basic_outcome(in_place_type_t<exception_type_if_enabled>, Args ...)`"
description = "Explicit inplace exception constructor. Available if `predicate::enable_inplace_exception_constructor<Args ...>` is true. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "explicit-constructors", "inplace-constructors"]
weight = 434
+++

Explicit inplace exception constructor. Calls {{% api "void hook_outcome_in_place_construction(T *, in_place_type_t<U>, Args &&...) noexcept" %}} with `this`, `in_place_type<exception_type>` and `Args ...`.

*Requires*: `predicate::enable_inplace_exception_constructor<Args ...>` is true.

*Complexity*: Same as for the `exception_type` constructor which accepts `Args ...`. Constexpr, triviality and noexcept of underlying operations is propagated.

*Guarantees*: If an exception is thrown during the operation, the state of the Args is left indeterminate.
