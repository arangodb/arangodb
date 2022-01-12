+++
title = "`static void _set_has_error(Impl &&, bool) noexcept`"
description = "Sets whether the implementation has an error. Constexpr, never throws."
categories = ["modifiers"]
weight = 310
+++

Sets whether the implementation has an error by setting or clearing the relevant bit in the flags. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
