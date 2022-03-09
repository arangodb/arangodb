+++
title = "`void outcome_throw_as_system_error_with_payload(const boost::system::error_code &)`"
description = "Specialisation of `outcome_throw_as_system_error_with_payload()` for `boost::system::error_code`."
+++

A specialisation of `outcome_throw_as_system_error_with_payload()` for `boost::system::error_code`. This executes {{% api "BOOST_OUTCOME_THROW_EXCEPTION(expr)" %}} with a `boost::system::system_error` constructed from the input.

*Overridable*: Argument dependent lookup.

*Requires*: Nothing.

*Namespace*: `boost::system`

*Header*: `<boost/outcome/boost_result.hpp>`
