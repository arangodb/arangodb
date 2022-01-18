+++
title = "`static void _set_has_value(Impl &&, bool) noexcept`"
description = "Sets whether the implementation has a value. Constexpr, never throws."
categories = ["modifiers"]
weight = 300
+++

Sets whether the implementation has a value by setting or clearing the relevant bit in the flags. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
