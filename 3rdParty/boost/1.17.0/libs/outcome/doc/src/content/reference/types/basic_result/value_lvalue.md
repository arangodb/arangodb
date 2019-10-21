+++
title = "`value_type &value() &`"
description = "Wide contract lvalue reference observer of any value present. Constexpr propagating."
categories = ["observers"]
weight = 640
+++

Wide contract lvalue reference observer of any value present. `NoValuePolicy::wide_value_check()` is first invoked, then the reference to the value is returned.

Note that if `value_type` is `void`, only a `const` overload returning `void` is present.

*Requires*: Always available.

*Complexity*: Depends on `NoValuePolicy::wide_value_check()`.

*Guarantees*: None.
