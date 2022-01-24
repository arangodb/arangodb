+++
title = "`boost_result<T, E = boost::system::error_code, NoValuePolicy = policy::default_policy<T, E, void>>`"
description = "A type alias to a `basic_result` configured with `boost::system::error_code` and `policy::default_policy`."
+++

A type alias to a {{% api "basic_result<T, E, NoValuePolicy>" %}} configured with `boost::system::error_code` and `policy::`{{% api "default_policy" %}}.

This type alias always references the `boost` edition of things, unlike {{% api "result<T, E = varies, NoValuePolicy = policy::default_policy<T, E, void>>" %}} which references either this alias or {{% api "std_result<T, E = std::error_code, NoValuePolicy = policy::default_policy<T, E, void>>" %}}.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/boost_result.hpp>`
