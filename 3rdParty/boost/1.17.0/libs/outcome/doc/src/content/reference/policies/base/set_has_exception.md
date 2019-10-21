+++
title = "`static void _set_has_exception(Impl &&, bool) noexcept`"
description = "Sets whether the implementation has an exception. Constexpr, never throws."
categories = ["modifiers"]
weight = 330
+++

Sets whether the implementation has an exception by setting or clearing the relevant bit in the flags. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
