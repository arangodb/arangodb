+++
title = "`terminate`"
description = "Policy class defining that `std::terminate()` should be called on incorrect wide value, error or exception observation. Inherits publicly from `base`."
+++

Policy class defining that {{% api "std::terminate()" %}} should be called on incorrect wide value, error or exception observation.

Inherits publicly from {{% api "base" %}}, and its narrow value, error and exception observer policies are inherited from there.

Included by `<basic_result.hpp>`, and so is always available when `basic_result` is available.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::policy`

*Header*: `<boost/outcome/policy/terminate.hpp>`
