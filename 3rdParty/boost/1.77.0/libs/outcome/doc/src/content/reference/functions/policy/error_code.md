+++
title = "`decltype(auto) error_code(T &&)`"
description = "Extracts a `boost::system::error_code` or `std::error_code` from the input via ADL discovery of a suitable `make_error_code(T)` function."
+++

Extracts a `boost::system::error_code` or {{% api "std::error_code" %}} from the input via ADL discovery of a suitable `make_error_code(T)` function.

*Overridable*: Argument dependent lookup.

*Requires*: Always available.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::policy`

*Header*: `<boost/outcome/std_result.hpp>`
