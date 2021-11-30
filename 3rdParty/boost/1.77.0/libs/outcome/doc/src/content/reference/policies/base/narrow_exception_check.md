+++
title = "`static void narrow_exception_check(Impl &&) noexcept`"
description = "Observer policy performing hard UB if no exception is present. Constexpr, never throws."
categories = ["observer-policies"]
weight = 420
+++

Observer policy performing hard UB if no exception is present, by calling {{% api "static void _ub(Impl &&)" %}}. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
