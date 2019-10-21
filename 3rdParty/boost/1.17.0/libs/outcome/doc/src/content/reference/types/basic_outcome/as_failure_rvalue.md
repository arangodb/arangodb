+++
title = "`failure_type<error_type, exception_type> as_failure() &&`"
description = "Return the output from free function `failure()` containing a move of any errored and/or excepted state."
categories = ["modifiers"]
weight = 920
+++

Return the output from free function {{% api "auto failure(T &&, ...)" %}} containing a move from any errored and/or excepted state, thus leaving the outcome's error and exception values in a moved-from state. Depending on the choice of `error_type` and/or `exception_type`, this function may therefore be destructive. The error and exception states are accessed using {{% api "error_type &&assume_error() && noexcept" %}} and {{% api "exception_type &&assume_exception() && noexcept" %}}.

*Requires*: Always available.

*Complexity*: Whatever that of `error_type`'s and/or `exception_type`'s move constructor is.

*Guarantees*: None.
