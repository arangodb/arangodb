+++
title = "`const error_type &assume_error() const & noexcept`"
description = "Narrow contract const lvalue reference observer of the stored error. Constexpr propagating, never throws."
categories = ["observers"]
weight = 710
+++

Narrow contract const lvalue reference observer of the stored error. `NoValuePolicy::narrow_error_check()` is first invoked, then the reference to the error is returned. As a valid default constructed error is always present, no undefined behaviour occurs unless `NoValuePolicy::narrow_error_check()` does that.

Note that if `error_type` is `void`, only a `const` overload returning `void` is present.

*Requires*: Always available.

*Complexity*: Depends on `NoValuePolicy::narrow_error_check()`.

*Guarantees*: An exception is never thrown.
