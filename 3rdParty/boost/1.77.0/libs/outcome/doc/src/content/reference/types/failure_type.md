+++
title = "`failure_type<EC, EP = void>`"
description = "Type sugar for constructing an unsuccessful result or outcome."
+++

Type sugar for constructing an unsuccessful result or outcome. Generally not constructed directly, but via the free function {{% api "failure(T &&, ...)" %}}. Detectable using {{% api "is_failure_type<T>" %}}.

This is a regular wrapper type, with defaulted default, copy and move constructor, defaulted assignment, and defaulted destructor.

Member type aliases `error_type` and `exception_type` indicate `EC` and `E`.

There is an explicit initialising constructor taking any types `U` and `V` which will forward construct the contained `error_type` and `exception_type` respectively. It also has an optional parameter `spare_storage`, if you wish to specify a spare storage value.

There are two tagged initialising constructors taking `in_place_type_t<error_type>` or `in_place_type_t<exception_type>`, and a `U` which will forward construct the contained `error_type` and `exception_type` respectively. These both also have an optional parameter `spare_storage`, if you wish to specify a spare storage value.

There are `.error()` and `.exception()` reference observers with the usual constexpr lvalue, const lvalue, rvalue and const rvalue overloads. One can discover which or both of these is valid using the usual `.has_error()` and `.has_exception()` observers.

There is a `.spare_storage()` observer which returns the spare storage value with which the failure type sugar was constructed.

There are specialisations `failure_type<EC, void>` and `failure_type<void, E>` which store nothing for the voided type and do not provide their observer functions.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/success_failure.hpp>`
