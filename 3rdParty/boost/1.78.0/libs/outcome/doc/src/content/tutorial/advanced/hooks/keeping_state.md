+++
title = "Keeping state"
description = ""
weight = 10
tags = ["backtrace"]
+++

The first thing we are going to need is somewhere to store the stack backtrace.
We could take the easier route and simply store it into an allocated block and
keep the pointer as a custom payload in a `result<T, std::pair<error_code, std::unique_ptr<stack_backtrace>>>`
(see previous section on [Custom payloads](../../payload)). But let us assume that we care so deeply about bounded execution times
that ever calling `malloc` is unacceptable.

We therefore are going to need some completely static and trivially typed storage
perhaps kept per-thread to avoid the need to keep mutexes.

{{% snippet "error_code_extended.cpp" "error_code_extended1" %}}

The extended error info is kept in a sixteen item long, thread local, ring buffer. We continuously
increment the current index pointer which is a 16 bit value which will wrap after
65,535. This lets us detect an attempt to access recycled storage, and thus return
item-not-found instead of the wrong extended error info.