+++
title = "`auto as_failure() &&`"
description = "Return the output from free function `failure()` containing a move of any errored state."
categories = ["modifiers"]
weight = 920
+++

Return the output from free function {{% api "auto failure(T &&, ...)" %}} containing a move from any errored state, thus leaving the result's error value in a moved-from state. Depending on the choice of `error_type`, this function may therefore be destructive. The error state is accessed using {{% api "error_type &&assume_error() && noexcept" %}}.

*Requires*: Always available.

*Complexity*: Whatever that of `error_type`'s move constructor is.

*Guarantees*: None.
