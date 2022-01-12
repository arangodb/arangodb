+++
title = "Built-in policies"
description = ""
weight = 20
+++

These are the predefined policies built into Outcome:

<hr style="border-color: black;">

&nbsp;

{{< api "all_narrow" >}}

If there is an observation of a value/error/exception which is not present,
the program is put into a **hard undefined behaviour** situation. The
compiler *literally* compiles no code for an invalid observation -- the
CPU "runs off" into the unknown.

As bad as this may sound, it generates the most optimal code, and such
hard UB is very tool-friendly for detection e.g. undefined behaviour
sanitiser, valgrind memcheck, etc.

If you are considering choosing this policy, definitely read
{{% api "static void _ub(Impl &&)" %}} first.

Note that {{< api "unchecked<T, E = varies>" >}} aliases a `basic_result`
with the `all_narrow` no-value policy.

<hr style="border-color: black;">

&nbsp;

{{< api "terminate" >}}

Observation of a missing value/error/exception causes a call to
`std::terminate()`.

Note that configuring `EC = void` or `EP = void` causes
{{% api "default_policy" %}} to choose `terminate` as the no-value policy.

<hr style="border-color: black;">

&nbsp;

{{< api "error_code_throw_as_system_error<T, EC, EP>" >}}

This policy assumes that `EC` has the interface of `std::error_code`,
and `EP` has the interface of `std::exception_ptr`. Upon missing value
observation:

* if an exception is stored through pointer of type `EP` it is rethrown;
* otherwise, if an error of type `EC` is stored, it is converted to `error_code`
  and then thrown as `system_error`.

Upon missing error observation throws:

* `bad_result_access("no error")` from instances of `basic_result<>`.
* `bad_outcome_access("no error")` from instances of `basic_outcome<>`.

Upon missing exception observation throws `bad_outcome_access("no exception")`.

Overloads are provided for `boost::system::error_code` and `boost::exception_ptr`.

Note that if {{% api "is_error_code_available<T>" %}} is true for `EC`,
and (if `basic_outcome`) {{% api "is_exception_ptr_available<T>" %}}
is true for `EP`, {{% api "default_policy" %}} chooses
`error_code_throw_as_system_error<T, EC, EP>` as the no-value policy.

<hr style="border-color: black;">

&nbsp;

{{< api "exception_ptr_rethrow<T, EC, EP>" >}}

This policy assumes that either `EC` or `EP` (unless `void`) has the interface of `std::exception_ptr`. Upon missing value observation:

* in instances of `basic_result<>`, rethrows exception pointed to by `EC`;
* in instances of `basic_outcome<>`, if exception `EP` is present rethrows it,
  otherwise rethrows `EC`.

Upon missing error observation:

* in instances of `basic_result<>`, throws  `bad_result_access("no error")` ;
* in instances of `basic_outcome<>`, throws  `bad_outcome_access("no error")`.

Upon missing exception observation throws `bad_outcome_access("no exception")`.

Overloads are provided for `boost::exception_ptr`.

Note that if {{% api "is_exception_ptr_available<T>" %}} is true for `EC`,
or (if `basic_outcome`) {{% api "is_exception_ptr_available<T>" %}}
is true for `EP`, {{% api "default_policy" %}} chooses
`exception_ptr_rethrow<T, EC, EP>` as the no-value policy.

<hr style="border-color: black;">

&nbsp;

{{< api "throw_bad_result_access<EC>" >}}

Upon missing value observation throws `bad_result_access_with<EC>(ec)`,
where `ec` is the value of the stored error. If error is not stored,
the behaviour is undefined.

Upon missing error observation throws `bad_result_access("no error")`.

This policy can be used with `basic_outcome<>` instances, where it always
throws `bad_outcome_access` for all no-value/error/exception observations.

Note that {{< api "checked<T, E = varies>" >}} aliases a `basic_result`
with the `throw_bad_result_access<EC>` no-value policy.
