+++
title = "`checked<T, E = varies>`"
description = "A type alias to a `std_checked<T, E>` (standalone edition) or `boost_checked<T, E>` (Boost edition)."
+++

A type alias to either {{% api "std_checked<T, E = std::error_code>" %}} (standalone edition) or {{% api "boost_checked<T, E = boost::system::error_code>" %}} (Boost edition). This means that `checked<T>` uses the appropriate default alias depending on which edition of Outcome is in use.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/result.hpp>`
