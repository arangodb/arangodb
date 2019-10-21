+++
title = "Hook outcome"
description = ""
weight = 50
+++

The final step is to add ADL discovered event hooks for the very specific case of
when our localised `outcome` is copy or move constructed from our localised `result`.

You ought to be very careful that the `noexcept`-ness of these matches the `noexcept`-ness
of the types in the `outcome`. You may have noticed that `poke_exception()` creates
a `std::string` and appends to it. This can throw an exception. If the copy and/or
move constructors of `T`, `EC` and `EP` are `noexcept`, then so will be `outcome`'s
copy and/or move constructor. Thus if `poke_exception()` throws, instant program
termination would occur, which is bad.

We avoid that problem in this case by wrapping `poke_exception()` in a `try...catch`
which throws away any exceptions thrown.

{{% snippet "error_code_extended.cpp" "error_code_extended5" %}}

