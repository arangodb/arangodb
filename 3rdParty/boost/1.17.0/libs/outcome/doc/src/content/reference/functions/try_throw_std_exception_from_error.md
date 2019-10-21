+++
title = "`void try_throw_std_exception_from_error(std::error_code ec, const std::string &msg = std::string{})`"
description = "Try to throw a standard library exception type matching an error code."
+++

This function saves writing boilerplate by throwing a standard library exception
type equivalent to the supplied error code, with an optional custom message.

If the function returns, there is no standard library exception type equivalent
to the supplied error code. The following codes produce the following exception
throws:

<dl>
<dt><code>EINVAL</code>
<dd><code>std::invalid_argument</code>
<dt><code>EDOM</code>
<dd><code>std::domain_error</code>
<dt><code>E2BIG</code>
<dd><code>std::length_error</code>
<dt><code>ERANGE</code>
<dd><code>std::out_of_range</code>
<dt><code>EOVERFLOW</code>
<dd><code>std::overflow_error</code>
<dt><code>ENOMEM</code>
<dd><code>std::bad_alloc</code>
</dl>

The choice to refer to POSIX `errno` values above reflects the matching algorithm.
As {{% api "std::errc" %}} exactly maps POSIX `errno`, on all platforms
{{% api "std::generic_category" %}} error codes are matched by this function.
Only on POSIX platforms only are {{% api "std::system_category" %}} error codes
also matched by this function.

*Overridable*: Not overridable.

*Requires*: C++ exceptions to be globally enabled.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/utils.hpp>`
