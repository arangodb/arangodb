+++
title = "`basic_result &operator=(const basic_result &)`"
description = "Copy assignment. Constexpr, triviality and noexcept propagating."
categories = ["operators", "assignment", "copy-assignment"]
weight = 140
+++

Copy assignment.

*Requires*: that `value_type` and `error_type` both implement copy assignment.

*Complexity*: If the `value_type` for both is present, uses `value_type`'s copy assignment operator, else either destructs or copy constructs `value_type` as appropriate. `error_type`'s copy assignment operator is always used. Constexpr, triviality and noexcept of underlying operations is propagated.

*Guarantees*: If an exception is thrown during the operation, the object is left in a partially completed state, as per the normal rules for the same operation on a `struct`.
