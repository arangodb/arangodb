+++
title = "`exception_type failure() const noexcept`"
description = "Synthesising observer of the stored exception or error. Available if the traits `is_error_code_available<T>` and `is_exception_ptr_available<T>` are both true. Never throws."
categories = ["observers"]
weight = 790
+++

Synthesising observer of the stored exception or error. If an exception is available,
returns a copy of that directly. If an error is available, and not an exception, an
ADL discovered free function {{% api "auto basic_outcome_failure_exception_from_error(const EC &)" %}}
is invoked. Default overloads for this function are defined in Outcome for {{% api "std::error_code" %}}
and `boost::system::error_code`, these return `std::make_exception_ptr(std::system_error(ec))`
and `boost::copy_exception(boost::system::system_error(ec))` respectively.

*Requires*: Both the traits {{% api "is_error_code_available<T>" %}} and
{{% api "is_exception_ptr_available<T>" %}} are true.

*Complexity*: Depends on `basic_outcome_failure_exception_from_error(const EC &)`.

*Guarantees*: Never throws. If an exception is thrown during the copy of the exception,
that exception (from `std::current_exception()`) is returned instead.
