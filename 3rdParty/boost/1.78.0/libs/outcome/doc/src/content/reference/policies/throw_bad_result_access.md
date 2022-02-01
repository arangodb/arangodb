+++
title = "`throw_bad_result_access<EC>`"
description = "Policy class defining that `bad_result_access_with<EC>` should be thrown on incorrect wide value observation. Inherits publicly from `base`."
+++

Policy class defining that {{% api "bad_result_access_with<EC>" %}} should be thrown on incorrect wide value observation. The primary purpose of this policy is to enable standing in for {{% api "std::expected<T, E>" %}} which throws a `bad_expected_access<E>` on incorrect wide value observation. This is why it is only ever `EC` which is thrown with `bad_result_access_with<EC>` on value observation only, and only when there is an error available.

If used in `basic_outcome`, and the outcome is exceptioned and so no error is available, incorrect wide value observation performs instead:

```c++
BOOST_OUTCOME_THROW_EXCEPTION(bad_result_access("no value"));
```

Incorrect wide error observation performs:

```c++
BOOST_OUTCOME_THROW_EXCEPTION(bad_result_access("no error"));
```

Incorrect wide exception observation performs:

```c++
BOOST_OUTCOME_THROW_EXCEPTION(bad_outcome_access("no exception"));
```

Inherits publicly from {{% api "base" %}}, and its narrow value, error and exception observer policies are inherited from there.

*Requires*: Nothing.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::policy`

*Header*: `<boost/outcome/policy/throw_bad_result_access.hpp>`
