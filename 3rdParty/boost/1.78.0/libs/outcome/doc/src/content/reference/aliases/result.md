+++
title = "`result<T, E = varies, NoValuePolicy = policy::default_policy<T, E, void>>`"
description = "A type alias to a `std_result<T, E, NoValuePolicy>` (standalone edition) or `boost_result<T, E, NoValuePolicy>` (Boost edition)."
+++

A type alias to either {{% api "std_result<T, E = std::error_code, NoValuePolicy = policy::default_policy<T, E, void>>" %}} (standalone edition) or {{% api "boost_result<T, E = boost::system::error_code, NoValuePolicy = policy::default_policy<T, E, void>>" %}} (Boost edition), and `policy::`{{% api "default_policy" %}}. This means that `result<T>` uses the appropriate default alias depending on which edition of Outcome is in use.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/result.hpp>`
