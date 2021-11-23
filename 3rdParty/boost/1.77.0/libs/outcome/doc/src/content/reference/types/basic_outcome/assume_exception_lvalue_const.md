+++
title = "`const exception_type &assume_exception() const & noexcept`"
description = "Narrow contract const lvalue reference observer of the stored exception. Constexpr propagating, never throws."
categories = ["observers"]
weight = 781
+++

Narrow contract const lvalue reference observer of the stored exception. `NoValuePolicy::narrow_exception_check()` is first invoked, then the reference to the exception is returned. As a valid default constructed exception is always present, no undefined behaviour occurs unless `NoValuePolicy::narrow_exception_check()` does that.

Note that if `exception_type` is `void`, only a `const` overload returning `void` is present.

*Requires*: Always available.

*Complexity*: Depends on `NoValuePolicy::narrow_exception_check()`.

*Guarantees*: An exception is never thrown.
