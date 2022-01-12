+++
title = "`basic_outcome(const basic_outcome &)`"
description = "Copy constructor. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "implicit-constructors", "copy-constructors"]
weight = 120
+++

Copy constructor.

*Requires*: that `value_type`, `error_type` and `exception_type` all implement a copy constructor.

*Complexity*: Same as for `value_type`'s, `error_type`'s and `exception_type`'s copy constructors. Constexpr, triviality and noexcept of underlying operations is propagated.

*Guarantees*: If an exception is thrown during the operation, the object is left in a partially completed state, as per the normal rules for the same operation on a `struct`.
