+++
title = "`bool operator==(const failure_type<A, void> &) const`"
description = "Returns true if this result compares equal to the failure type sugar. Constexpr and noexcept propagating."
categories = ["comparisons"]
weight = 820
+++

Returns true if this result is unsuccessful and its error compares equal to the error in the failure type sugar. Comparison is done using `operator==` on `error_type` and `A`. If `A` is `void`, this call aliases {{% api "bool has_error() const noexcept" %}}.

*Requires*: `operator==` must be a valid expression between `error_type` and `A`, or `A` is `void`. If `error_type` is `void`, then so must be `A`.

*Complexity*: Whatever the underlying `operator==` has. Constexpr and noexcept of underlying operations is propagated.

*Guarantees*: None.
