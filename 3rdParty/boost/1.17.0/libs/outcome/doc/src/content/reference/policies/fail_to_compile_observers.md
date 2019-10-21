+++
title = "`fail_to_compile_observers`"
description = "Policy class defining that a static assertion should occur upon compilation of the wide value, error or exception observation. Inherits publicly from `base`."
+++

Upon attempting to compile the wide observer policy functions, the following static assertion occurs which fails the build:

> *Attempt to wide observe value, error or exception for a result/outcome given an EC or E type which is not void, and for whom trait::has_error_code_v<EC>, trait::has_exception_ptr_v<EC>, and trait::has_exception_ptr_v<E> are all false. Please specify a NoValuePolicy to tell result/outcome what to do, or else use a more specific convenience type alias such as unchecked<T, E> to indicate you want the wide observers to be narrow, or checked<T, E> to indicate you always want an exception throw etc.*

This failure to compile was introduced after the Boost peer review of v2.0 of Outcome due to feedback that users were too often surprised by the default selection of the {{% api "all_narrow" %}} policy if the types were unrecognised. It was felt this introduced too much danger in the default configuration, so to ensure that existing code based on Outcome broke very loudly after an upgrade, the above very verbose static assertion was implemented.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::policy`

*Header*: `<boost/outcome/policy/fail_to_compile_observers.hpp>`
