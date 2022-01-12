+++
title = "`static auto &&_value(Impl &&) noexcept`"
description = "Returns a reference to the value in the implementation passed in. Constexpr, never throws."
categories = ["observers"]
weight = 250
+++

Returns a reference to the value in the implementation passed in. No checking is done to ensure there is a value. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
