+++
title = "`error_code_throw_as_system_error<T, EC, EP>`"
description = "Policy class defining that `EP` ought to be rethrown if possible, then the ADL discovered free function `outcome_throw_as_system_error_with_payload()` should be called on incorrect wide value observation. Inherits publicly from `base`. Can only be used with `basic_outcome`."
+++

*Note*: This policy class specialisation can only be used with `basic_outcome`, not `basic_result`. Use {{% api "error_code_throw_as_system_error<T, EC, void>" %}} with `basic_result`.

Policy class defining that on incorrect wide value observation, `EP` ought to be rethrown if possible, then the ADL discovered free function `outcome_throw_as_system_error_with_payload(impl.assume_error())` should be called. [Some precanned overloads of that function are listed here]({{< relref "/reference/functions/policy" >}}).

Incorrect wide error observation performs:

```c++
BOOST_OUTCOME_THROW_EXCEPTION(bad_outcome_access("no error"));
```

Incorrect wide exception observation performs:

```c++
BOOST_OUTCOME_THROW_EXCEPTION(bad_outcome_access("no exception"));
```

Inherits publicly from {{% api "base" %}}, and its narrow value, error and exception observer policies are inherited from there.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::policy`

*Header*: `<boost/outcome/policy/outcome_error_code_throw_as_system_error.hpp>`
