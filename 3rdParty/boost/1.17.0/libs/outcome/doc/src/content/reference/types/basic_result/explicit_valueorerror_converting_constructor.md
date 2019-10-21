+++
title = "`explicit basic_result(ValueOrError<T, E> &&)`"
description = "Explicit converting constructor from `ValueOrError<T, E>` concept matching types. Available if `convert::value_or_error<>` permits it. Constexpr, triviality and noexcept propagating."
categories = ["constructors", "explicit-constructors", "converting-constructors"]
weight = 300
+++

Explicit converting constructor from {{% api "ValueOrError<T, E>" %}} concept matching types. Delegates to the `basic_result` move constructor.

*Requires*: `convert::`{{% api "value_or_error<T, U>" %}} has an available call operator, and if the input is a `basic_result` or `basic_outcome`, then `convert::value_or_error<>` has enabled those inputs for that `convert::value_or_error<>` specialisation.

*Complexity*: Same as for the copy or move constructor from the input's `.value()` or `.error()` respectively. Constexpr, triviality and noexcept of underlying operations is propagated.

*Guarantees*: If an exception is thrown during the operation, the object is left in a partially completed state, as per the normal rules for the same operation on a `struct`.
