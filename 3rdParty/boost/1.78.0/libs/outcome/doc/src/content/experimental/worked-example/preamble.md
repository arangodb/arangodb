+++
title = "Define a custom code domain"
weight = 10
+++

Firstly let's alias the experimental Outcome namespace into something
less tedious to type, declare our custom status code type, and get
started on defining the custom status code domain implementation.

{{% snippet "experimental_status_code.cpp" "preamble" %}}

Note that we inherit from `outcome_e::posix_code::domain_type`, not
from `outcome_e::status_code_domain`. We thus reuse most of the
implementation of `outcome_e::posix_code::domain_type` rather than
implementing required functionality. If you would like to see a
fuller treatment of defining a custom status code domain from
scratch, see [this worked example here](https://github.com/ned14/status-code/blob/master/doc/custom_domain_worked_example.md).

[You may find looking at the API reference documentation for `status_code_domain`
useful in the next few pages ](https://ned14.github.io/status-code/doc_status_code_domain.html#standardese-system_error2__status_code_domain).