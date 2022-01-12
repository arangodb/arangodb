+++
title = "`exception_ptr_rethrow<T, EC, void>`"
description = "Policy class defining that the ADL discovered free function `rethrow_exception()` should be called on incorrect wide value observation. Inherits publicly from `base`. Can only be used with `basic_result`."
+++

*Note*: This policy class specialisation can only be used with `basic_result`, not `basic_outcome`. Use {{% api "exception_ptr_rethrow<T, EC, EP>" %}} with `basic_outcome`.

Policy class defining that the ADL discovered free function `rethrow_exception(impl.assume_error())` should be called on incorrect wide value observation. Generally this will ADL discover {{% api "std::rethrow_exception()" %}} or `boost::rethrow_exception()` depending on the `EC` type.

Incorrect wide error observation performs:

```c++
BOOST_OUTCOME_THROW_EXCEPTION(bad_result_access("no error"));
```

Inherits publicly from {{% api "base" %}}, and its narrow value, error and exception observer policies are inherited from there.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::policy`

*Header*: `<boost/outcome/policy/result_exception_ptr_rethrow.hpp>`
