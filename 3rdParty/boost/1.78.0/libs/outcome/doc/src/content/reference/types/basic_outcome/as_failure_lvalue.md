+++
title = "`failure_type<error_type, exception_type> as_failure() const &`"
description = "Return the output from free function `failure()` containing a copy of any errored and/or excepted state."
categories = ["observers"]
weight = 910
+++

Return the output from free function {{% api "auto failure(T &&, ...)" %}} containing a copy of any errored and/or excepted state. The error and/or exception state is accessed using {{% api "const error_type &assume_error() const & noexcept" %}} and {{% api "const exception_type &assume_exception() const & noexcept" %}}.

*Requires*: Always available.

*Complexity*: Whatever that of `error_type`'s and/or `exception_type`'s copy constructor is.

*Guarantees*: None.
