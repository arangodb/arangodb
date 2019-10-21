+++
title = "`auto failure(T &&, ...)`"
description = "Returns appropriate type sugar for constructing an unsuccessful result or outcome."
+++

Returns appropriate type sugar for constructing an unsuccessful result or outcome, usually {{% api "failure_type<EC, EP = void>" %}} with a decayed `T`.

Two default overloads are provided, one taking a single parameter returning `failure_type<std::decay_t<T>>`, the other taking two parameters returning `failure_type<std::decay_t<T>, std::decay_t<U>>`. Both overloads perfectly forward their inputs.

Note that `failure()` overloads are permitted by Outcome to return something other than `failure_type`. For example, `basic_result`'s {{% api "auto as_failure() const &" %}} returns whatever type `failure()` returns, and {{% api "BOOST_OUTCOME_TRY(var, expr)" %}} by default returns for failure whatever `.as_failure()` returns. This can be useful to have `BOOST_OUTCOME_TRY(...}` propagate on failure something custom for some specific input `basic_result` or `basic_outcome`.

*Overridable*: By Argument Dependent Lookup (ADL).

*Requires*: Always available.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/success_failure.hpp>`
