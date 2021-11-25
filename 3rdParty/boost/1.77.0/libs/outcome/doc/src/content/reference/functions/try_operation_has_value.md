+++
title = "`bool try_operation_has_value(X)`"
description = "Default implementation of `try_operation_has_value(X)` ADL customisation point for `BOOST_OUTCOME_TRY`."
+++

This default implementation returns whatever the `.has_value()` member function returns.

*Requires*: That the expression `std::declval<T>().has_value()` is a valid expression.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/try.hpp>`
