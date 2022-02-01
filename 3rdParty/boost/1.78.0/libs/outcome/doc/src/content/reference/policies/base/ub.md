+++
title = "`static void _ub(Impl &&)`"
description = "Special function which causes noticeable hard undefined behaviour."
categories = ["special"]
weight = 100
+++

This is a special function which does compiler-specific stuff to tell the compiler that this function can never, ever, ever be executed. The compiler's optimiser will **hard assume** that this function can never be executed, and will prune the possibility of it being executed completely. Generally this means that the code path stops dead, and if execution does proceed down this path, it will run off the end of a branch which doesn't go anywhere. Your program will have lost known state, and usually it will trash memory and registers and crash.

This may seem highly undesirable. However, it also means that the optimiser can optimise more strongly, and so long as you never actually do execute this branch, you do get higher quality code generation.

If the `NDEBUG` macro is not defined, an `assert(false)` is present. This will cause attempts to execute this function to fail in a very obvious way, but it also generates runtime code to trigger the obvious failure.

If the `NDEBUG` macro is defined, and the program is compiled with the undefined behaviour sanitiser, attempts to execute this function will trigger an undefined behaviour sanitiser action.

*Requires*: Always available.

*Complexity*: Zero runtime overhead if `NDEBUG` is defined, guaranteed. If this function returns, your program is now in hard loss of known program state. *Usually*, but not always, it will crash at some point later. *Rarely* it will corrupt registers and memory, and keep going.

*Guarantees*: An exception is never thrown.
