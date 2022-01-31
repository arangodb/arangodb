+++
title = "Phase 2 construction"
description = ""
weight = 30
+++

Its phase 2 constructor:

{{% snippet "constructors.cpp" "file" %}}

The static member function implementing phase 2 firstly calls phase 1
which puts the object into a legally destructible state. We then
proceed to implement phase 2 of construction, filling in the various
parts as we go, reporting via `result` any failures.

{{% notice note %}}
Remember that `operator new` has a non-throwing form, `new(std::nothrow)`.
{{% /notice %}}

For the final return, in theory we could just `return ret` and
depending on the C++ version currently in force, it might work
via move, or via copy, or it might refuse to compile. You can
of course type lots of boilerplate to be explicit, but this use
via initialiser list is a reasonable balance of explicitness
versus brevity, and it should generate minimum overhead code
irrespective of compiler, C++ version, or any other factor.
