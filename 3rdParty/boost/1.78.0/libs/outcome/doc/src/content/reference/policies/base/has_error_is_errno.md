+++
title = "`static bool _has_error_is_errno(Impl &&) noexcept`"
description = "Returns true if the error code in the implementation passed in has a domain or category matching that of POSIX `errno`. Constexpr, never throws."
categories = ["observers"]
weight = 240
+++

Returns true if the error code in the implementation passed in has a domain or category matching that of POSIX `errno`. Constexpr where possible.

*Requires*: Always available.

*Complexity*: Constant time.

*Guarantees*: Never throws an exception.
