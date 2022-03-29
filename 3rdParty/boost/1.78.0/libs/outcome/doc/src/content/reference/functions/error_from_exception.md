+++
title = "`std::error_code error_from_exception(std::exception_ptr &&ep = std::current_exception(), std::error_code not_matched = std::make_error_code(std::errc::resource_unavailable_try_again)) noexcept`"
description = "Returns an error code matching a thrown standard library exception."
+++

This function saves writing boilerplate by rethrowing `ep` within a `try`
block, with a long sequence of `catch()` handlers, one for every standard
C++ exception type which has a near or exact equivalent code in {{% api "std::errc" %}}.

If matched, `ep` is set to a default constructed {{% api "std::exception_ptr" %}},
and a {{% api "std::error_code" %}} is constructed using the ADL discovered free
function `make_error_code()` upon the `std::errc` enumeration value matching the
thrown exception.

If not matched, `ep` is left intact, and the `not_matched` error code supplied
is returned instead.

*Overridable*: Not overridable.

*Requires*: C++ exceptions to be globally enabled.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/utils.hpp>`
