+++
title = "Upgrading the Filesystem TS"
description = ""
weight = 20
tags = ["payload"]
+++

An Outcome based solution to the dual overload problem is straightforward:

{{% snippet "outcome_payload.cpp" "filesystem_api_fixed" %}}

Starting at the bottom, there is now a single `copy_file()` function which returns a `fs_result<void>`.
As `result` is either successful or not, there is no longer any point in returning a boolean, so we
simply return `void` on success. On failure, as the template alias `fs_result<T>` above it shows,
we are returning a `failure_info` structure containing an error code and the same additional information
as `filesystem_error` provides.

It is important to note that the fact that `failure_info` is defined in namespace `filesystem2` is very
important. This is because Outcome uses [Argument Dependent Lookup (ADL)](http://en.cppreference.com/w/cpp/language/adl)
to find the `make_error_code()`
function, as well as other customisation point free functions. In other words, only the namespaces as
defined by ADL are searched when finding a free function telling us what to do for `failure_info`,
which includes the namespace `failure_info` is declared into.
