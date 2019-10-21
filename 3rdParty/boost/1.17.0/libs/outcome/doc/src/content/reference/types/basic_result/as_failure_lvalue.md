+++
title = "`auto as_failure() const &`"
description = "Return the output from free function `failure()` containing a copy of any errored state."
categories = ["observers"]
weight = 910
+++

Return the output from free function {{% api "auto failure(T &&, ...)" %}} containing a copy of any errored state. The error state is accessed using {{% api "const error_type &assume_error() const & noexcept" %}}.

*Requires*: Always available.

*Complexity*: Whatever that of `error_type`'s copy constructor is.

*Guarantees*: None.
