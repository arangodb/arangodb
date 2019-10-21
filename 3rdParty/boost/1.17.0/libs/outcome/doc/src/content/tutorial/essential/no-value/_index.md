+++
title = "No-value policies"
description = "Describes the concept of NoValuePolicy and how to use no-value policies."
weight = 25
tags = [ "policies" ]
+++

In the previous section we have seen that it would be useful if calling member
function `.value()` on object of type `outcome<T>` that did not contain a value,
would cause an exception to be thrown according to some user-defined policy.

Let us consider `result<T>` first. It is an alias to {{% api "basic_result<T, E, NoValuePolicy>" %}},
where `E` is the type storing error information and defaulted to
`std::error_code`/`boost::system::error_code`, and `NoValuePolicy`
is a *no-value policy* defaulted to {{% api "default_policy<T, EC, EP>" %}}.

The semantics of `basic_result::value()` are:

1. Calls `NoValuePolicy::wide_value_check(*this)`.
2. Return a reference to the contained value. If no value is actually stored,
your program has entered undefined behaviour.

Thus, the semantics of function `.value()` depend on the no-value policy. The
default policy (`policy::default_policy<T, EC, void>`) for `EC` of type
`std::error_code`[^1] does the following:

* If `r.has_value() == false` throws exception `std::system_error{r.error()}`,
* otherwise no effect.

{{% notice note %}}
Class templates {{% api "basic_result<T, E, NoValuePolicy>" %}} and {{% api "basic_outcome<T, EC, EP, NoValuePolicy>" %}}
never use exceptions. Any exception-related logic is provided exclusively
through no-value policies.
{{% /notice %}}

When designing your own success-or-failure type using templates
`basic_result<>` or `basic_outcome<>` you have to decide what no-value policy
you want to use. Either create your own, or [use one of the predefined policies]({{< relref "/tutorial/essential/no-value/builtin" >}}).

You can also use one of the two other predefined aliases for `basic_result<>`:

* {{< api "unchecked<T, E = varies>" >}}: it uses policy {{< api "all_narrow" >}}, where any observation of a missing value or error is undefined behavior;
* {{< api "checked<T, E = varies>" >}}:
  it uses policy {{< api "throw_bad_result_access<EC>" >}}, where any observation of a missing value or error throws {{< api "bad_result_access_with<EC>" >}} or {{< api "bad_result_access" >}}
  respectively.

[^1]: Similar overloads exist for throwing `boost::system::system_error` when `EC` is `boost::system::error_code`.
