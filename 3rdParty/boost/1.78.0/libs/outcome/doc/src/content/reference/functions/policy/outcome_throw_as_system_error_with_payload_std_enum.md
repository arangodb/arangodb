+++
title = "`void outcome_throw_as_system_error_with_payload(ErrorCodeEnum &&)`"
description = "Specialisation of `outcome_throw_as_system_error_with_payload()` for input types where `std::is_error_code_enum<ErrorCodeEnum>` or `std::is_error_condition_enum<ErrorCodeEnum>` is true."
+++

A specialisation of `outcome_throw_as_system_error_with_payload()` for types where `std::is_error_code_enum<ErrorCodeEnum>` or `std::is_error_condition_enum<ErrorCodeEnum>` is true. This executes {{% api "BOOST_OUTCOME_THROW_EXCEPTION(expr)" %}} with a {{% api "std::system_error" %}} constructed from the result of the ADL discovered free function `make_error_code(ErrorCodeEnum)`.

*Overridable*: Argument dependent lookup.

*Requires*: Either {{% api "std::is_error_code_enum<T>" %}} or {{% api "std::is_error_condition_enum<T>" %}} to be true for a decayed `ErrorCodeEnum`.

*Namespace*: `std`

*Header*: `<boost/outcome/std_result.hpp>`
