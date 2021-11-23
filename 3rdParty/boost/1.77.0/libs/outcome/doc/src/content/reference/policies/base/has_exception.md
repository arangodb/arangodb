+++
title = "`static bool _has_exception(Impl &&) noexcept`"
description = "Returns true if an exception is present in the implementation passed in. Constexpr, never throws."
categories = ["observers"]
weight = 230
+++

Returns true if an exception is present in the implementation passed in. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
