+++
title = "`unchecked<T, E = varies>`"
description = "A type alias to a `std_unchecked<T, E>` (standalone edition) or `boost_unchecked<T, E>` (Boost edition)."
+++

A type alias to either {{% api "std_unchecked<T, E = std::error_code>" %}} (standalone edition) or {{% api "boost_unchecked<T, E = boost::system::error_code>" %}} (Boost edition). This means that `unchecked<T>` uses the appropriate default alias depending on which edition of Outcome is in use.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/result.hpp>`
