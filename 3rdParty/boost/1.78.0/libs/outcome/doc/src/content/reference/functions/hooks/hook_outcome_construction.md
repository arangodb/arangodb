+++
title = "`void hook_outcome_construction(T *, U &&) noexcept`"
description = "(Until v2.2.0) ADL discovered free function hook invoked by the implicit constructors of `basic_outcome`."
+++

Removed in Outcome v2.2.0, unless {{% api "BOOST_OUTCOME_ENABLE_LEGACY_SUPPORT_FOR" %}} is set to less than `220` to
enable emulation. Use {{% api "on_outcome_construction(T *, U &&) noexcept" %}} instead in new code.

One of the constructor hooks for {{% api "basic_outcome<T, EC, EP, NoValuePolicy>" %}}, generally invoked by the implicit constructors of `basic_outcome`. See each constructor's documentation to see which specific hook it invokes.

*Overridable*: By Argument Dependent Lookup.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::hooks`

*Header*: `<boost/outcome/basic_outcome.hpp>`
