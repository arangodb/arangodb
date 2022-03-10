+++
title = "`basic_outcome<T>`"
description = "A boolean concept matching types which are convertible to a `basic_outcome<T, EC, EP, NoValuePolicy>`."
+++

If on C++ 20 or the Concepts TS is enabled, a boolean concept matching types which have `value_type`, `error_type` and `no_value_policy_type` member typedefs; that the type is convertible to `basic_result<value_type, error_type, no_value_policy_type>`; that `basic_result<value_type, error_type, no_value_policy_type>` is a base of the type.

If without Concepts, a static constexpr bool which is true for types matching the same requirements, using a SFINAE based emulation.

This concept matches any type which provides the same typedefs as a {{% api "basic_result<T, E, NoValuePolicy>" %}}, has that `basic_result` as a base class, and is implicitly convertible to `basic_result`. Whilst not guaranteed, it is very likely that the type is a `basic_result`, or inherits publicly from a `basic_result`. If you want something which matches any value-or-error type, consider {{% api "value_or_error<T>" %}}. If you want something which exactly matches `basic_outcome`, use {{% api "is_basic_outcome<T>" %}}.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::concepts`

*Header*: `<boost/outcome/basic_result.hpp>`
