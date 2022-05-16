+++
title = "`bool has_value() const noexcept`"
description = "Returns true if a value is present. Constexpr, never throws."
categories = ["observers"]
weight = 591
+++

Returns true if a value is present. Constexpr where possible. Alias for {{% api "explicit operator bool() const noexcept" %}}.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
