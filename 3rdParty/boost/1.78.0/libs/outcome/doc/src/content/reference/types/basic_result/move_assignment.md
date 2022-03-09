+++
title = "`basic_result &operator=(basic_result &&)`"
description = "Move assignment. Constexpr, triviality and noexcept propagating."
categories = ["operators", "assignment", "move-assignment"]
weight = 130
+++

Move assignment.

*Requires*: that `value_type` and `error_type` both implement move assignment.

*Complexity*: If the `value_type` for both is present, uses `value_type`'s move assignment operator, else either destructs or move constructs `value_type` as appropriate. `error_type`'s move assignment operator is always used. Constexpr, triviality and noexcept of underlying operations is propagated.

*Guarantees*: If an exception is thrown during the operation, the object is left in a partially completed state, as per the normal rules for the same operation on a `struct`.
