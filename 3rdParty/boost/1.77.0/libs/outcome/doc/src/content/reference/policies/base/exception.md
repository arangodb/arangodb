+++
title = "`static auto &&_exception(Impl &&) noexcept`"
description = "Returns a reference to the exception in the implementation passed in. Constexpr, never throws."
categories = ["observers"]
weight = 270
+++

Returns a reference to the exception in the implementation passed in. No checking is done to ensure there is an error. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
