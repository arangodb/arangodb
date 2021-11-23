+++
title = "`success_type<T>`"
description = "Type sugar for constructing a successful result or outcome."
+++

Type sugar for constructing a successful result or outcome. Generally not constructed directly, but via the free function {{% api "success(T &&, ...)" %}}. Detectable using {{% api "is_success_type<T>" %}}.

This is a regular wrapper type, with defaulted default, copy and move constructor, defaulted assignment, and defaulted destructor.

A member type alias `value_type` indicates `T`.

There is an explicit initialising constructor taking any type `U` which is not a `success_type<T>`, and which will forward construct the contained `T` from that `U`. It also has an optional parameter `spare_storage`, if you wish to specify a spare storage value.

There is a `.value()` reference observer with the usual constexpr lvalue, const lvalue, rvalue and const rvalue overloads.

There is a specialisation `success_type<void>` which stores nothing and provides no `.value()` observers.

There is a `.spare_storage()` observer which returns the spare storage value with which the success type sugar was constructed.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/success_failure.hpp>`
