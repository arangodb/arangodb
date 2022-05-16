+++
title = "`BOOST_OUTCOME_USE_STD_IN_PLACE_TYPE`"
description = "How to implement `in_place_type_t<T>` and `in_place_type<T>`."
+++

How to implement {{% api "in_place_type_t<T>" %}} and `in_place_type<T>`.

If set to `1`, the `<utility>` header is included, and `std::in_place_type_t<T>`
is aliased into `BOOST_OUTCOME_V2_NAMESPACE::in_place_type_t<T>` along with
`std::in_place_type<T>`.

If set to `0`, a local emulation is used.

*Overridable*: Define before inclusion.

*Default*: If the current compiler implements C++ 17 or later, if unset
this macro is defaulted to `1`, otherwise it is defaulted to `0`.

*Header*: `<boost/outcome/config.hpp>`