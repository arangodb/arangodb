+++
title = "Redefining `message()`"
weight = 50
+++

You may remember that our custom `_file_io_error_domain` inherits from
`outcome_e::posix_code::domain_type`, and thus does not have to
implement the many pure virtual functions required by `outcome_e::status_code_domain`.

What we do need to do is reimplement `_do_message()` to append the
file and line information to the POSIX error description string
returned by `outcome_e::posix_code::domain_type`. This causes
the status code's `.message()` observer to return a string
with the extra payload information represented in text.

{{% snippet "experimental_status_code.cpp" "message" %}}

 