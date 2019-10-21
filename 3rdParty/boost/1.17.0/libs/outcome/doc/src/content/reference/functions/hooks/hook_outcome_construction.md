+++
title = "`void hook_outcome_construction(T *, U &&) noexcept`"
description = "ADL discovered free function hook invoked by the implicit constructors of `basic_outcome`."
+++

One of the constructor hooks for {{% api "basic_outcome<T, EC, EP, NoValuePolicy>" %}}, generally invoked by the implicit constructors of `basic_outcome`. See each constructor's documentation to see which specific hook it invokes.

*Overridable*: By Argument Dependent Lookup.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::hooks`

*Header*: `<boost/outcome/basic_outcome.hpp>`
