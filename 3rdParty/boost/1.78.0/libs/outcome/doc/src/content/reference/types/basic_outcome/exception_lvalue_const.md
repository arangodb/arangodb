+++
title = "`const exception_type &exception() const &`"
description = "Wide contract const lvalue reference observer of the stored exception. Constexpr propagating."
categories = ["observers"]
weight = 786
+++

Wide contract const lvalue reference observer of the stored exception. `NoValuePolicy::wide_exception_check()` is first invoked, then the reference to the exception is returned. As a valid default constructed exception is always present, no undefined behaviour occurs if `NoValuePolicy::wide_exception_check()` returns.

Note that if `exception_type` is `void`, only a `const` overload returning `void` is present.

*Requires*: Always available.

*Complexity*: Depends on `NoValuePolicy::wide_exception_check()`.

*Guarantees*: None.
