+++
title = "result<>"
description = "Gentle introduction to writing code with simple success-or-failure return types."
weight = 10
tags = ["result", "try", "namespace"]
+++

We will define a function that converts a `std::string` to an `int`. This function can fail for a number of reasons;
if it does we want to communicate the failure reason.

{{% snippet "using_result.cpp" "convert_decl" %}}

Template alias {{< api "result<T, E = varies, NoValuePolicy = policy::default_policy<T, E, void>>" >}}
has three template parameters, but the last two have default values. The first
(`T`) represents the type of the object returned from the function upon success.
The second (`EC`) is the type of object containing information about the reason
for failure when the function fails. A result object stores either a `T` or an
`EC` at any given moment, and is therefore conceptually similar to `variant<T, EC>`.
`EC` is defaulted to `std::error_code` in standalone Outcome, or to `boost::system::error_code`
in Boost.Outcome[^1]. The third parameter (`NoValuePolicy`) is called a
no-value policy. We will cover it later.

If both `T` and `EC` are [trivially copyable](https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable), `result<T, EC, NVP>` is also trivially copyable.
Triviality, complexity and constexpr-ness of each operation (construction, copy construction, assignment,
destruction etc) is always the intersection of the corresponding operation in `T` and `EC`,
so for example if both `T` and `EC` are [literal types](https://en.cppreference.com/w/cpp/named_req/LiteralType), so will be `result<T, EC, NVP>`.
Additionally, if both `T` and `EC` have [standard layout](https://en.cppreference.com/w/cpp/named_req/StandardLayoutType), `result<T, EC, NVP>` has standard layout;
thus if both `T` and `EC` are C-compatible, so will `result<T, EC, NVP>` be C-compatible.

Now, we will define an enumeration describing different failure situations during conversion.

{{% snippet "using_result.cpp" "enum" %}}

Assume we have plugged it into `std::error_code` framework, as described in [this section](../../../motivation/plug_error_code).

One notable effect of such plugging is that `ConversionErrc` is now convertible to `std::error_code`.
Now we can implement function `convert` as follows:

{{% snippet "using_result.cpp" "convert" %}}

`result<T, EC>` is convertible from any `T2` convertible to `T` as well as any `EC2` convertible to `EC`,
provided that there is no constructability possible in either direction between `T` and `EC`. If there is,
all implicit conversion is disabled, and you will need to use one of the tagged constructors:

{{% snippet "using_result.cpp" "explicit" %}}

Or use helper functions:

{{% snippet "using_result.cpp" "factory" %}}

{{% notice note %}}
The functions {{< api "auto failure(T &&, ...)" >}} and {{< api "auto success(T &&)" >}} return special
types implicitly convertible to failed or successful `result` (and `outcome`).
{{% /notice %}}

[^1]: You can mandate a choice using `std_result<T>` or `boost_result<T>`.
