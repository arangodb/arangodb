+++
title = "`bad_result_access`"
description = "Exception type publicly inheriting from `std::logic_error` indicating an incorrect observation of value or error occurred."
+++

Exception type publicly inheriting from {{% api "std::logic_error" %}} indicating an incorrect observation of value or error occurred by {{% api "basic_result<T, E, NoValuePolicy>" %}}.

No member functions are added in addition to `std::logic_error`. Typical `.what()` strings are:

- `"no value"`
- `"no error"`

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/bad_access.hpp>`
