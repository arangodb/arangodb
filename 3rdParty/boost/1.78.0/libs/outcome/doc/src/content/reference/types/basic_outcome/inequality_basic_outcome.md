+++
title = "`bool operator!=(const basic_outcome<A, B, C, D> &) const`"
description = "Returns true if this outcome does not compare equal to the other outcome. Constexpr and noexcept propagating."
categories = ["comparisons"]
weight = 846
+++

Returns true if this outcome does not compare equal to the other outcome. Comparison is done using `operator!=` on `value_type`, `error_type` and/or `exception_type` if the currently chosen state is the same for both outcomes, otherwise true is returned.

*Requires*: `operator!=` must be a valid expression between `value_type` and `A`, and between `error_type` and `B`, and between `exception_type` and `C`. If `value_type` is `void`, then so must be `A`; similarly for `error_type` and `B`; similarly for `exception_type` and `C`.

*Complexity*: Whatever the underlying `operator!=` have. Constexpr and noexcept of underlying operations is propagated.

*Guarantees*: None.
