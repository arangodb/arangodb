+++
title = "`in_place_type_t<T>`"
description = "Either `std::in_place_type_t<T>` or a local emulation, depending on the `BOOST_OUTCOME_USE_STD_IN_PLACE_TYPE` macro."
+++

Either `std::in_place_type_t<T>` or a local emulation, depending on the
{{% api "BOOST_OUTCOME_USE_STD_IN_PLACE_TYPE" %}} macro.

Note that the templated variable `in_place_type` is also aliased or emulated locally.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/config.hpp>`
