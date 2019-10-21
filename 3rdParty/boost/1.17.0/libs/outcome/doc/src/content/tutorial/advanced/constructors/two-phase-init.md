+++
title = "Two phase construction"
description = ""
weight = 10
+++

The first thing to do is to break your object's construction into two phases:

1. Place the object into a state where it can be legally destructed
without doing any initialisation which could throw an exception (i.e. everything
done in phase 1 is `constexpr`). This phase usually involves initialising member
variables to various default values, most often using default member initialisers.
Most standard C++ library objects
and containers have `constexpr` constructors, and thus can be initialised
during phase 1. If you need to initialise a member variable without
a `constexpr` constructor, {{% api "std::optional<T>" %}} is the usual workaround.

2. Do the remainder of the construction, the parts which could fail.
Because phase 1 placed the object into a legally destructible state,
it is usually the case that one can bail out during phase 2 and the
destructor will clean things up correctly.

The phase 1 construction will be placed into a *private* `constexpr`
constructor so nobody external can call it. The phase 2 construction will be placed into a static
member initialisation function which returns a `result` with either
the successfully constructed object, or the cause of any failure to
construct the object.

Finally, as a phase 3,
some simple metaprogramming will implement a `make<T>{Args...}()`
free function which will construct any object `T` by calling its
static initialisation function with `Args...` and returning the
`result` returned. This isn't as nice as calling `T(Args...)` directly,
but it's not too bad in practice. And more importantly, it lets you
write generic code which can construct any unknown object which
fails via returning `result`, completely avoiding C++ exception throws.
