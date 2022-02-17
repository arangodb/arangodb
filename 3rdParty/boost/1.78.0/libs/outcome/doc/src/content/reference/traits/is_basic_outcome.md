+++
title = "`is_basic_outcome<T>`"
description = "An integral constant type true for `basic_outcome<T, EC, EP, NoValuePolicy>` types."
+++

An integral constant type true for {{% api "basic_outcome<T, EC, EP, NoValuePolicy>" %}} types. This does not match anything not exactly a `basic_outcome`. If you want to match types like `basic_outcome` but not equal to it, consider {{% api "basic_outcome<T>" %}}..

*Overridable*: Not overridable.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/basic_outcome.hpp>`

*Variable alias*: `is_basic_outcome_v<T>`
