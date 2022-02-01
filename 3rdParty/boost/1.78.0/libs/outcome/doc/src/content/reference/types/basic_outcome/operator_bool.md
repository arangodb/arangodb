+++
title = "`explicit operator bool() const noexcept`"
description = "Returns true if a value is present. Constexpr, never throws."
categories = ["observers"]
weight = 590
+++

Returns true if a value is present. Constexpr where possible. Alias for {{% api "bool has_value() const noexcept" %}}.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
