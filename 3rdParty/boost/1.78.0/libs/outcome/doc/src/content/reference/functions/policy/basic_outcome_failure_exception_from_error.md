+++
title = "`auto basic_outcome_failure_exception_from_error(const EC &)`"
description = "ADL discovered free function synthesising an exception type from an error type, used by the `.failure()` observers."
+++

Synthesises an exception type from an error type, used by the {{% api "exception_type failure() const noexcept" %}}
observers. ADL discovered. Default
overloads for this function are defined in Outcome for {{% api "std::error_code" %}}
and `boost::system::error_code`, these return `std::make_exception_ptr(std::system_error(ec))`
and `boost::copy_exception(boost::system::system_error(ec))` respectively.

*Overridable*: Argument dependent lookup.

*Requires*: Nothing.

*Namespace*: Namespace of `EC` type.

*Header*: `<boost/outcome/std_outcome.hpp>`, `<boost/outcome/boost_outcome.hpp>`
