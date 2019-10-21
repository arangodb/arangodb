+++
title = "`std_checked<T, E = std::error_code>`"
description = "A type alias to a `basic_result` configured with `std::error_code` and `policy::throw_bad_result_access<EC>`."
+++

A type alias to a {{% api "basic_result<T, E, NoValuePolicy>" %}} configured with {{% api "std::error_code" %}} and `policy::`{{% api "throw_bad_result_access<EC>" %}}.

This type alias always references the `std` edition of things, unlike {{% api "checked<T, E = varies>" %}} which references either this alias or {{% api "boost_checked<T, E = boost::system::error_code>" %}}.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/std_result.hpp>`
