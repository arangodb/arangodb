+++
title = "`static void narrow_error_check(Impl &&) noexcept`"
description = "Observer policy performing hard UB if no error is present. Constexpr, never throws."
categories = ["observer-policies"]
weight = 410
+++

Observer policy performing hard UB if no error is present, by calling {{% api "static void _ub(Impl &&)" %}}. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
