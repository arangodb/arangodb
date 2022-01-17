+++
title = "The HTTP library"
weight = 10
+++

Let us imagine a simple application: it fetches a HTTP page using a HTTP library,
sends it through HTML tidy via the htmltidy library, and then writes it to disc
using a filelib library. So three third party libraries, two using Outcome in
incompatible ways, and the third being a C library just for kicks.

Let us imagine that the HTTP library has the following public interface:

{{% snippet "finale.cpp" "httplib" %}}

The HTTP library is a mixed-failure design. Likely failures (HTTP status codes)
are returned via `httplib::failure`, unlikely failures (e.g. out of memory)
are returned via throw of the usual STL exception types.

The sole API we bother describing is an implementation of HTTP GET. It fetches
a URL, and either returns the contents or the failure reason why not.
