+++
title = "`static void on_outcome_in_place_construction(T *, in_place_type_t<U>, Args &&...) noexcept`"
description = "(>= Outcome v2.2.0) Hook invoked by the in-place constructors of `basic_outcome`."
categories = ["observer-policies"]
weight = 450
+++

One of the constructor hooks for {{% api "basic_outcome<T, EC, EP, NoValuePolicy>" %}}, generally invoked by the in-place constructors of `basic_outcome`. See each constructor's documentation to see which specific hook it invokes.

*Requires*: Always available.

*Guarantees*: Never throws an exception.
