+++
title = "The Application"
weight = 25
+++

The application is of course also based on Outcome, and like the HTTP library
is also of mixed-failure design in that failure can be returned via error code,
type erased `exception_ptr` or indeed a C++ exception throw.

{{% snippet "finale.cpp" "app" %}}

Here we localise a passthrough `error_code` solely for the purpose of ADL bridging, otherwise
the localised `outcome` configured is the default one which comes with Outcome.
[We covered this technique of "passthrough `error_code`" earlier in this tutorial]({{< relref "/tutorial/advanced/hooks/adl_bridging" >}}).

The way we are going to configure interop is as follows:

1. The application shall use `error_code` for anticipated failure and C++
exception throws for unanticipated failure.
2. We shall choose the convention that `app::outcome` with exception ptr
solely and exclusively represents a type erased failure from a third party
library.

Thus if one calls `.value()` on an `app::outcome`, both anticipated failure
within the app and type erased failure from a third party library shall be
converted to a C++ exception throw.