+++
title = "`void swap(basic_outcome &)`"
description = "Swap one basic_outcome with another, with the strong guarantee. Noexcept propagating."
categories = ["modifiers"]
weight = 900
+++

Swap one basic_outcome with another, with the strong guarantee. Constexpr if move constructors and move assignments of `value_type`, `error_type` and `exception_type` are noexcept.

*Requires*: Always available.

*Complexity*: If the move constructor and move assignment for `value_type`, `error_type` and `exception_type` are noexcept, the complexity is the same as for the `swap()` implementations of the `value_type`, `error_type` and `exception_type`. Otherwise, complexity is not preserved, as {{% api "strong_swap(bool &all_good, T &a, T &b)" %}} is used instead of `swap()`. This function defaults to using one move construction and two assignments, and it will attempt extra move assignments in order to restore the state upon entry if a failure occurs.

*Guarantees*: If an exception is thrown during the swap operation, the state of all three operands on entry is attempted to be restored, in order to implement the strong guarantee. If that too fails, the flag bits are forced to something consistent such that there can be no simultaneously valued and errored/excepted state, or valueless and errorless/exceptionless. The flag {{% api "has_lost_consistency()" %}} becomes true for both operands, which are now likely in an inconsistent state.
