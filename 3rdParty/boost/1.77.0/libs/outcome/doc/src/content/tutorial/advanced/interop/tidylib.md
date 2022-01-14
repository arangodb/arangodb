+++
title = "The HTMLTidy library"
weight = 15
+++

{{% snippet "finale.cpp" "tidylib" %}}

A C API may not initially appear to be a `T|E` based API, but if failure
returns some domained error code and causes no other effects, and success
returns some value, then it is effectively a "split" `T|E` API. The above
is an example of exactly that form of "split" `T|E` API.