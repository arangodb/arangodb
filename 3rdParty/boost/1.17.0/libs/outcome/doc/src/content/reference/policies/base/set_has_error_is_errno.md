+++
title = "`static void _set_has_exception(Impl &&, bool) noexcept`"
description = "Sets whether the implementation's error code has a domain or category matching that of POSIX `errno`. Constexpr, never throws."
categories = ["modifiers"]
weight = 340
+++

Sets whether the implementation's error code has a domain or category matching that of POSIX `errno` by setting or clearing the relevant bit in the flags. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
