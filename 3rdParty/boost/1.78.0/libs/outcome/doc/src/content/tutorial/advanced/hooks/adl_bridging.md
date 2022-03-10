+++
title = "ADL bridging"
description = ""
weight = 20
tags = [ "adl-bridging"]
+++

{{% notice note %}}
In Outcome v2.2 the ADL-based event hooks were replaced with policy-based event hooks (next page).
The code in this section is still valid in v2.2 onwards, it's just that ADL is no longer used
to find the hooks.
{{% /notice %}}

In a previous section, we used the `failure_info` type to create
the ADL bridge into the namespace where the ADL discovered [`outcome_throw_as_system_error_with_payload()`]({{< relref "/reference/functions/policy" >}}) function was to be found.

Here we do the same, but more directly by creating a thin clone of `std::error_code`
into the local namespace. This ensures that this namespace will be searched by the
compiler when discovering the event hooks (Outcome v2.1 and earlier only).

{{% snippet "error_code_extended.cpp" "error_code_extended2" %}}

For convenience, we template alias local copies of `result` and `outcome` in this
namespace bound to the ADL bridging `error_code`.