+++
title = "`void hook_outcome_copy_construction(T *, U &&, V &&) noexcept`"
description = "ADL discovered free function hook invoked by the converting copy constructors of `basic_outcome`."
+++

One of the constructor hooks for {{% api "basic_outcome<T, EC, EP, NoValuePolicy>" %}}, generally invoked by the converting copy constructors of `basic_outcome` (NOT the standard copy constructor) which consume two arguments. See each constructor's documentation to see which specific hook it invokes.

*Overridable*: By Argument Dependent Lookup.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::hooks`

*Header*: `<boost/outcome/basic_outcome.hpp>`
