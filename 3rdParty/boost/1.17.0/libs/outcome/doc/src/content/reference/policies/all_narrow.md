+++
title = "`all_narrow`"
description = "Policy class defining that hard undefined behaviour should occur on incorrect narrow and wide value, error or exception observation. Inherits publicly from `base`."
+++

Policy class defining that hard undefined behaviour should occur on incorrect narrow and wide value, error or exception observation.

Inherits publicly from {{% api "base" %}}, and simply defines its wide value, error and exception observer policies to call their corresponding narrow editions.

Included by `<basic_result.hpp>`, and so is always available when `basic_result` is available.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::policy`

*Header*: `<boost/outcome/policy/all_narrow.hpp>`
