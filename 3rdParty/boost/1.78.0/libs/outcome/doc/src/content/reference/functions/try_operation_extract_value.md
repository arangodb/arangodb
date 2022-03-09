+++
title = "`decltype(auto) try_operation_extract_value(X)`"
description = "Default implementation of `try_operation_extract_value(X)` ADL customisation point for `BOOST_OUTCOME_TRY`."
+++

This default implementation returns whatever the `.assume_value()` or `.value()` member functions return, preferentially choosing the former where both are available.

*Requires*: That the expression `std::declval<T>().assume_value()` and/or `std::declval<T>().value()` is a valid expression.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/try.hpp>`
