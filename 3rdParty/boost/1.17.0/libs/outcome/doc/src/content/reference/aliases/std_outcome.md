+++
title = "`std_outcome<T, EC = std::error_code, EP = std::exception_ptr, NoValuePolicy = policy::default_policy<T, EC, EP>>`"
description = "A type alias to a `basic_outcome` configured with `std::error_code`, `std::exception_ptr` and `policy::default_policy`."
+++

A type alias to a {{% api "basic_outcome<T, EC, EP, NoValuePolicy>" %}} configured with {{% api "std::error_code" %}}, {{% api "std::exception_ptr" %}} and `policy::`{{% api "default_policy" %}}.

This type alias always references the `std` edition of things, unlike {{% api "outcome<T, EC = varies, EP = varies, NoValuePolicy = policy::default_policy<T, EC, EP>>" %}} which references either this alias or {{% api "boost_outcome<T, EC = boost::system::error_code, EP = boost::exception_ptr, NoValuePolicy = policy::default_policy<T, EC, EP>>" %}}.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/std_outcome.hpp>`
