+++
title = "`bool operator==(const success_type<A> &) const`"
description = "Returns true if this result compares equal to the success type sugar. Constexpr and noexcept propagating."
categories = ["comparisons"]
weight = 810
+++

Returns true if this result is successful and its value compares equal to the value in the success type sugar. Comparison is done using `operator==` on `value_type` and `A`. If `A` is `void`, this call aliases {{% api "bool has_value() const noexcept" %}}.

*Requires*: `operator==` must be a valid expression between `value_type` and `A`, or `A` is `void`. If `value_type` is `void`, then so must be `A`.

*Complexity*: Whatever the underlying `operator==` has. Constexpr and noexcept of underlying operations is propagated.

*Guarantees*: None.
