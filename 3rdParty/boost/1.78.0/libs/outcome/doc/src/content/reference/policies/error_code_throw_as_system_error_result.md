+++
title = "`error_code_throw_as_system_error<T, EC, void>`"
description = "Policy class defining that the ADL discovered free function `outcome_throw_as_system_error_with_payload()` should be called on incorrect wide value observation. Inherits publicly from `base`. Can only be used with `basic_result`."
+++

*Note*: This policy class specialisation can only be used with `basic_result`, not `basic_outcome`. Use {{% api "error_code_throw_as_system_error<T, EC, EP>" %}} with `basic_outcome`.

Policy class defining that the ADL discovered free function `outcome_throw_as_system_error_with_payload(impl.assume_error())` should be called on incorrect wide value observation. [Some precanned overloads of that function are listed here]({{< relref "/reference/functions/policy" >}}).

Incorrect wide error observation performs:

```c++
BOOST_OUTCOME_THROW_EXCEPTION(bad_result_access("no error"));
```

Inherits publicly from {{% api "base" %}}, and its narrow value, error and exception observer policies are inherited from there.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::policy`

*Header*: `<boost/outcome/policy/result_error_code_throw_as_system_error.hpp>`
