+++
title = "`void outcome_throw_as_system_error_with_payload(const std::error_code &)`"
description = "Specialisation of `outcome_throw_as_system_error_with_payload()` for `std::error_code`."
+++

A specialisation of `outcome_throw_as_system_error_with_payload()` for `std::error_code`. This executes {{% api "BOOST_OUTCOME_THROW_EXCEPTION(expr)" %}} with a {{% api "std::system_error" %}} constructed from the input.

*Overridable*: Argument dependent lookup.

*Requires*: Nothing.

*Namespace*: `std`

*Header*: `<boost/outcome/std_result.hpp>`
