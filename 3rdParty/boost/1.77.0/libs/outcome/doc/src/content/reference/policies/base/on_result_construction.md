+++
title = "`static void on_result_construction(T *, U &&) noexcept`"
description = "(>= Outcome v2.2.0) Hook invoked by the implicit constructors of `basic_result`."
categories = ["observer-policies"]
weight = 450
+++

One of the constructor hooks for {{% api "basic_result<T, E, NoValuePolicy>" %}}, generally invoked by the implicit constructors of `basic_result`. See each constructor's documentation to see which specific hook it invokes.

*Requires*: Always available.

*Guarantees*: Never throws an exception.
