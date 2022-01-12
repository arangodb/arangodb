+++
title = "`auto success(T &&, ...)`"
description = "Returns appropriate type sugar for constructing a successful result or outcome."
+++

Returns appropriate type sugar for constructing a successful result or outcome, usually {{% api "success_type<T>" %}} with a decayed `T`.

Two default overloads are provided, one taking a single required parameter with optional spare storage value parameter returning `success_type<std::decay_t<T>>` and perfectly forwarding the input. The other overload takes no parameters, and returns `success_type<void>`, which usually causes the construction of the receiving `basic_result` or `basic_outcome`'s with a default construction of their value type.

*Overridable*: By Argument Dependent Lookup (ADL).

*Requires*: Always available.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/success_failure.hpp>`
