+++
title = "Example C++ function"
description = ""
weight = 20
+++

Let us start with a simple C++ function which we wish to make available to C code:

{{% snippet "cpp_api.cpp" "function" %}}

As the alias `status_result<size_t>` defaults the erased type to the alias `system_code`,
the `to_string()` function returns (in concrete types) `basic_result<size_t, status_code<erased<intptr_t>>>`.

The standard Outcome function referenced is documented at
{{% api "std::error_code error_from_exception(std::exception_ptr &&ep = std::current_exception(), std::error_code not_matched = std::make_error_code(std::errc::resource_unavailable_try_again)) noexcept" %}}.
The proposed `<system_error2>` reference library implementation provides an identically named
function taking similar parameters, but it returns a `outcome_e::system_code` (`status_code<erased<intptr_t>>`) instead of a `std::error_code`.

