+++
title = "`bool operator==(const basic_result<A, B, C> &) const`"
description = "Returns true if this result compares equal to the other result. Constexpr and noexcept propagating."
categories = ["comparisons"]
weight = 800
+++

Returns true if this result compares equal to the other result. Comparison is done using `operator==` on either `value_type` or `error_type` if the currently chosen state is the same for both results, otherwise false is returned.

*Requires*: `operator==` must be a valid expression between `value_type` and `A`, and between `error_type` and `B`. If `value_type` is `void`, then so must be `A`; similarly for `error_type` and `B`.

*Complexity*: Whatever the underlying `operator==` have. Constexpr and noexcept of underlying operations is propagated.

*Guarantees*: None.

*Free function alias*: There is a free function `bool operator==(const basic_result<A, B, C> &, const basic_outcome<A, B, C, D> &)` which forwards perfectly to this function, by reversing the operands.
