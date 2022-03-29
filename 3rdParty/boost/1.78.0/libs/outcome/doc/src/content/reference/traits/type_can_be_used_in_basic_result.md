+++
title = "`type_can_be_used_in_basic_result<R>`"
description = "A constexpr boolean true for types permissible in `basic_result<T, E, NoValuePolicy>`."
+++

A constexpr boolean true for types permissible in `basic_result<T, E, NoValuePolicy>`.

*Overridable*: Not overridable.

*Definition*: True for a type which:

- Is not a reference.
- Is not an {{% api "in_place_type_t<T>" %}}.
- Is not a {{% api "success_type<T>" %}}.
- Is not a {{% api "failure_type<EC, EP = void>" %}}.
- Is not an array.
- Is either `void`, or else is an `Object` and is `Destructible`.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::trait`

*Header*: `<boost/outcome/trait.hpp>`