+++
title = "`is_basic_result<T>`"
description = "An integral constant type true for `basic_result<T, E, NoValuePolicy>` types."
+++

An integral constant type true for {{% api "basic_result<T, E, NoValuePolicy>" %}} types. This does not match anything not exactly a `basic_result`. If you want to match types like `basic_result` but not equal to it, consider {{% api "basic_result<T>" %}} or {{% api "value_or_error<T>" %}}.

*Overridable*: Not overridable.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/basic_result.hpp>`

*Variable alias*: `is_basic_result_v<T>`
