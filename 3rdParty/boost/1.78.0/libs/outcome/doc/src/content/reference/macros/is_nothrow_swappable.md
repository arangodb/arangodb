+++
title = "`BOOST_OUTCOME_USE_STD_IS_NOTHROW_SWAPPABLE`"
description = "How to implement `is_nothrow_swappable<T>`."
+++

Whether to implement `is_nothrow_swappable<T>` as `std::is_nothrow_swappable<T>`,
or else use a local emulation.

*Overridable*: Define before inclusion.

*Default*: If the current compiler implements C++ 17 or later, if unset
this macro is defaulted to `1`, otherwise it is defaulted to `0`.

*Header*: `<boost/outcome/config.hpp>`