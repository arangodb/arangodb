+++
title = "`void strong_swap(bool &all_good, T &a, T &b)`"
description = "Tries to perform a strong guarantee swap."
+++

The standard `swap()` function provides the weak guarantee i.e. that no resources are lost. This ADL discovered function provides the strong guarantee instead: that if any of these operations throw an exception (i) move construct to temporary (ii) move assign `b` to `a` (iii) move assign temporary to `b`, an attempt is made to restore the exact pre-swapped state upon entry, and if that recovery too fails, then the boolean `all_good` will be false during stack unwind from the exception throw, to indicate that state has been lost.

This function is used within `basic_result::`{{% api "swap(basic_result &)" %}} if, and only if, either or both of `value_type` or `error_type` have a throwing move constructor or move assignment. It permits you to customise the implementation of the strong guarantee swap in Outcome with a more efficient implementation.

*Overridable*: By Argument Dependent Lookup (ADL).

*Requires*: That `T` is both move constructible and move assignable.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/basic_result.hpp>`
