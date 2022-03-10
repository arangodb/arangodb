+++
title = "`outcome<T, EC = varies, EP = varies, NoValuePolicy = policy::default_policy<T, EC, EP>>`"
description = "A type alias to a `std_outcome<T, EC, EP, NoValuePolicy>` (standalone edition) or `boost_outcome<T, EC, EP, NoValuePolicy>` (Boost edition)."
+++

A type alias to either {{% api "std_outcome<T, EC = std::error_code, EP = std::exception_ptr, NoValuePolicy = policy::default_policy<T, EC, EP>>" %}} (standalone edition) or {{% api "boost_outcome<T, EC = boost::system::error_code, EP = boost::exception_ptr, NoValuePolicy = policy::default_policy<T, EC, EP>>" %}} (Boost edition), and `policy::`{{% api "default_policy" %}}. This means that `outcome<T>` uses the appropriate default alias depending on which edition of Outcome is in use.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/outcome.hpp>`
