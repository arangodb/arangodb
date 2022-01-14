+++
title = "`bool has_lost_consistency() const noexcept`"
description = "Returns true if a preceding swap involving this object failed to preserve the strong guarantee. Constexpr, never throws."
categories = ["observers"]
weight = 596
+++

Returns true if a preceding swap involving this object failed to preserve the strong guarantee. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
