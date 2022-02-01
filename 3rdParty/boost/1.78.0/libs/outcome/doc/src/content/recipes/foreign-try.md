+++
title = "Extending `BOOST_OUTCOME_TRY`"
description = "How to informing `BOOST_OUTCOME_TRY` about foreign Result types."
tags = [ "TRY" ]
+++

Outcome's {{% api "BOOST_OUTCOME_TRY(var, expr)" %}} operation is fully extensible
to accept as input any foreign types.
It already recognises types matching the
{{% api "concepts::value_or_error<T, E>" %}} concept, which is to say all types which have:

- A public `.has_value()` member function which returns a `bool`.
- In order of preference, a public `.assume_value()`/`.value()` member
function.
- In order of preference, a public `.as_failure()`/`.assume_error()`/`.error()`
member function.

This should automatically handle inputs of `std::expected<T, E>`, and many others,
including intermixing Boost.Outcome and standalone Outcome within the same
translation unit.

`BOOST_OUTCOME_TRY` has the following free function customisation points:

<dl>
<dt><code>BOOST_OUTCOME_V2_NAMESPACE::</code>{{% api "try_operation_has_value(X)" %}}
<dd>Returns a `bool` which is true if the input to TRY has a value.
<dt><code>BOOST_OUTCOME_V2_NAMESPACE::</code>{{% api "try_operation_return_as(X)" %}}
<dd>Returns a suitable {{% api "failure_type<EC, EP = void>" %}} which
is returned immediately to cause stack unwind. Ought to preserve rvalue
semantics (i.e. if passed an rvalue, move the error state into the failure
type).
<dt><code>BOOST_OUTCOME_V2_NAMESPACE::</code>{{% api "try_operation_extract_value(X)" %}}
<dd>Extracts a value type from the input for the `TRY` to set its variable.
Ought to preserve rvalue semantics (i.e. if passed an rvalue, move the value).
</dl>

New overloads of these to support additional input types must be injected into
the `BOOST_OUTCOME_V2_NAMESPACE` namespace before the compiler parses the relevant
`BOOST_OUTCOME_TRY` in order to be found. This is called 'early binding' in the two
phase name lookup model in C++. This was chosen over 'late binding', where an
`BOOST_OUTCOME_TRY` in a templated piece of code could look up overloads introduced after
parsing the template containing the `BOOST_OUTCOME_TRY`, because it has much lower
impact on build times, as binding is done once at the point of parse, instead
of on every occasion at the point of instantiation. If you are careful to ensure
that you inject the overloads which you need early in the parse of the
translation unit, all will be well.

Let us work through an applied example.

---
## A very foreign pseudo-Expected type

This is a paraphrase of a poorly written pseudo-Expected type which I once
encountered in the production codebase of a large multinational. Lots
of the code was already using it, and it was weird enough that it couldn't
be swapped out for something better easily.

{{% snippet "foreign_try.cpp" "foreign_type" %}}

What we would like is for new code to be written using Outcome, but be able
to transparently call old code, like this:

{{% snippet "foreign_try.cpp" "functions" %}}

Telling Outcome about this weird foreign Expected is straightforward:

{{% snippet "foreign_try.cpp" "tell_outcome" %}}

And now `BOOST_OUTCOME_TRY` works exactly as expected:

{{% snippet "foreign_try.cpp" "example" %}}

... which outputs:

```
new_code(5) returns successful 5

new_code(0) returns failure argument out of domain
```
