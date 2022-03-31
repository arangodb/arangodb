+++
title = "`decltype(auto) exception_ptr(T &&)`"
description = "Extracts a `boost::exception_ptr` or `std::exception_ptr` from the input via ADL discovery of a suitable `make_exception_ptr(T)` function."
+++

Extracts a `boost::exception_ptr` or {{% api "std::exception_ptr" %}} from the input via ADL discovery of a suitable `make_exception_ptr(T)` function.

*Overridable*: Argument dependent lookup.

*Requires*: Always available.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::policy`

*Header*: `<boost/outcome/std_result.hpp>`
