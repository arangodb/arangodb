+++
title = "`bad_outcome_access`"
description = "Exception type publicly inheriting from `std::logic_error` indicating an incorrect observation of value or error or exception occurred."
+++

Exception type publicly inheriting from {{% api "std::logic_error" %}} indicating an incorrect observation of value or error or exception occurred by {{% api "basic_outcome<T, EC, EP, NoValuePolicy>" %}}.

No member functions are added in addition to `std::logic_error`. Typical `.what()` strings are:

- `"no value"`
- `"no error"`
- `"no exception"`

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/bad_access.hpp>`
