+++
title = "Implicit conversion"
weight = 70
+++

Back in [The payload]({{< relref "/experimental/worked-example/value_type" >}}), we
mentioned that there was no default implicit conversion of `file_io_error`
(`status_code<_file_io_error_domain>`) to `error`, as `error` is too small
to hold `_file_io_error_domain::value_type`.

We can tell the framework about available implicit conversions by defining
an ADL discovered free function `make_status_code()` which takes our
custom status code as input, and returns an `error`:

{{% snippet "experimental_status_code.cpp" "implicit_conversion" %}}

We are now ready to use Experimental Outcome!
