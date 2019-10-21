+++
title = "`decltype(auto) try_operation_return_as(X)`"
description = "Default implementation of `try_operation_return_as(X)` ADL customisation point for `BOOST_OUTCOME_TRY`."
+++

This default implementation preferentially returns whatever the input type's `.as_failure()` member function returns.
`basic_result` and `basic_outcome` provide such a member function, see {{% api "auto as_failure() const &" %}}.

If `.as_failure()` is not available, it will also match any `.error()` member function, which it wraps into a failure type sugar using {{% api "failure(T &&, ...)" %}}.

*Requires*: That the expression `std::declval<T>().as_failure()` and/or `std::declval<T>().error()` is a valid expression.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/try.hpp>`
