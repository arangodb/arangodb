+++
title = "value_or_error Concept"
weight = 7
tags = [ "value-or-error" ]
+++

Something not really mentioned until now is how Outcome interoperates with the proposed
{{% api "std::expected<T, E>" %}}, whose design lands in between {{% api "unchecked<T, E = varies>" %}}
and {{% api "checked<T, E = varies>" %}} (both of which are type aliases hard coding no-value
policies [as previously covered in this tutorial]({{< relref "/tutorial/essential/no-value/builtin" >}})).

Expected and Outcome are [isomorphic to one another in design intent]({{< relref "/faq" >}}#why-doesn-t-outcome-duplicate-std-expected-t-e-s-design), but interoperation
for code using Expected and Outcome ought to be seamless thanks to the [proposed `ValueOrError`
concept framework](https://wg21.link/P0786), a subset of which Outcome implements.

The {{% api "explicit basic_result(concepts::value_or_error<T, E> &&)" %}} and {{% api "explicit basic_outcome(concepts::value_or_error<T, E> &&)" %}}
constructors will explicitly construct from any type matching the {{% api "concepts::value_or_error<T, E>" %}}
concept, which includes `std::expected<A, B>`, if `A` is constructible to `X`, and `B` is
constructible to `Y`. The `value_or_error` concept in turn is true if and only if the input type has:

1. A `.has_value()` observer returning a `bool`.
2. `.value()` and `.error()` observers.

## Implementation

Outcome's machinery for implementing `concepts::value_or_error` conversion is user extensible by injection
of specialisations of the {{% api "value_or_error<T, U>" %}} type into the `BOOST_OUTCOME_V2_NAMESPACE::convert` namespace.

Outcome's default `convert::value_or_error<T, U>` implementation explicitly
excludes Outcome `result` and `outcome` types from the default mechanism as
there is a major gotcha: the `value_or_error` matched type's `.value()` is often
not callable in constexpr as it can throw, which makes this conversion mechanism
pretty much useless for constexpr code. Besides, `outcome` has a converting
constructor overload for `result` inputs which is constexpr capable.

Note that if you do enable `outcome` inputs, a `result` will match an input
`outcome`, but silently dropping any exception state. This is probably undesirable.

Examples of how to implement your own `convert::value_or_error<T, U>` converter
is demonstrated in the worked example, next.