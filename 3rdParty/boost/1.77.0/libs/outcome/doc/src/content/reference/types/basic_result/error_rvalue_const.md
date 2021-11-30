+++
title = "`const error_type &&error() const &&`"
description = "Wide contract const rvalue reference observer of the stored error. Constexpr propagating."
categories = ["observers"]
weight = 770
+++

Wide contract const rvalue reference observer of the stored error. `NoValuePolicy::wide_error_check()` is first invoked, then the reference to the error is returned. As a valid default constructed error is always present, no undefined behaviour occurs if `NoValuePolicy::wide_error_check()` returns.

Note that if `error_type` is `void`, only a `const` overload returning `void` is present.

*Requires*: Always available.

*Complexity*: Depends on `NoValuePolicy::wide_error_check()`.

*Guarantees*: None.
