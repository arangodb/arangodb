+++
title = "Constexpr domain source"
weight = 60
+++

Back in [The constructor]({{< relref "/experimental/worked-example/constructor" >}}), we
declared but did not implement a `.get()` function which returns a constexpr static
instance of the domain. We implement this now:

{{% snippet "experimental_status_code.cpp" "constexpr_source" %}}

As this is 100% constexpr, it can be (and is under optimisation) implemented entirely
in the mind of the compiler with no run time representation.
