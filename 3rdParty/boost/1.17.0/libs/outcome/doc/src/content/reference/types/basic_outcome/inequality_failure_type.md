+++
title = "`bool operator!=(const failure_type<A, B> &) const`"
description = "Returns true if this outcome does not compare equal to the failure type sugar. Constexpr and noexcept propagating."
categories = ["comparisons"]
weight = 860
+++

Returns true if this outcome is successful or its error or exception does not compare equal to the error in the failure type sugar. Comparison is done using `operator!=` on `error_type` and `A` and on `exception_type` and `B`.

*Requires*: `operator!=` must be a valid expression between `error_type` and `A`, or `A` is `void`; `operator!=` must be a valid expression between `exception_type` and `B`, or `B` is `void`. If `error_type` is `void`, then so must be `A`; if `exception_type` is `void`, then so must be `B`.

*Complexity*: Whatever the underlying `operator!=` has. Constexpr and noexcept of underlying operations is propagated.

*Guarantees*: None.
