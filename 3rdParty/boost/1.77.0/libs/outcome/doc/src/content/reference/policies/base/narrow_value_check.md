+++
title = "`static void narrow_value_check(Impl &&) noexcept`"
description = "Observer policy performing hard UB if no value is present. Constexpr, never throws."
categories = ["observer-policies"]
weight = 400
+++

Observer policy performing hard UB if no value is present, by calling {{% api "static void _ub(Impl &&)" %}}. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
