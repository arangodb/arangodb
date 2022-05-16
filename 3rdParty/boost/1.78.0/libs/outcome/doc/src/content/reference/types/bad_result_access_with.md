+++
title = "`bad_result_access_with<EC>`"
description = "Exception type publicly inheriting from `bad_result_access` indicating an incorrect observation of value occurred, supplying the error value."
+++

Exception type publicly inheriting from {{% api "bad_result_access" %}}, and thus also {{% api "std::logic_error" %}}, indicating an incorrect observation of value occurred. The error value at the time of the exception throw is moved or copied into this type, and is available using the `.error()` observer which comes in lvalue ref, const lvalue ref, rvalue ref, and const rvalue ref overloads.

The primary purpose of this exception type is to enable standing in for {{% api "std::expected<T, E>" %}}'s `bad_expected_access<E>` which is thrown on incorrect wide value observation. This is why it is only ever `EC` which is thrown with `bad_result_access_with<EC>` on value observation only, and only when there is an error available. See the {{% api "throw_bad_result_access<EC>" %}} policy for more information.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE`

*Header*: `<boost/outcome/bad_access.hpp>`
