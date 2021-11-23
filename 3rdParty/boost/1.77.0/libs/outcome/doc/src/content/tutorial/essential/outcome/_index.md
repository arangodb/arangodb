+++
title = "outcome<>"
description = "Success-or-failure return types where failure can take two forms, expected/handled failure and unexpected/abort failure."
weight = 20
tags = ["outcome"]
+++


Type {{< api "outcome<T, EC = varies, EP = varies, NoValuePolicy = policy::default_policy<T, EC, EP>>" >}} represents either a successfully computed value of type `T`, or one or two reasons for failure. Failure can be represented by `EC`, or `EP`, or both, although usually it will either be an `EC` or an `EP`.
Similarly to `result`, `EC` defaults to `std::error_code`/`boost::system::error_code`, and `EP` defaults to `std::exception_ptr`/`boost::exception_ptr`.

The distinction is made into two types, `EC` and `EP`:

- `EC` represents a *recoverable* failure from a lower-layer function, perhaps which was returned through {{< api "result<T, E = varies, NoValuePolicy = policy::default_policy<T, E, void>>" >}}.
- `EP` represents an *unrecoverable* failure where a C++ exception would ordinarily have been thrown. This is usually a pointer to an exception throw.

It should be emphasised that this distinction is by convention only, but it will be confusing to your
users if you deviate significantly from this convention.

<hr>

### Legacy codebases

`outcome` is useful for transporting exceptions across layers of the program that were never designed with exception safety in mind.

Consider a program consisting of three layers:

{{<if_boost "/tutorial/essential/outcome/layer_chart.gif">}}
{{<mermaid>}}
graph BT
    L3["Layer3"]
    L2["Layer2_old"] --> L3
    L1["Layer1"] --> L2
{{</mermaid>}}
{{</if_boost>}}

The highest-level layer, `Layer3`, uses exceptions for signalling failures. The middle layer, `Layer2_old`,
was not designed with exception safety in mind and functions need to return information about failures in return value.
But down in the implementation details, in `Layer1`, another library is used that again throws exceptions. The goal is
to be able to transfer an exception thrown in `Layer1` through `Layer2_old`, which is not exception-safe,
and be able to rethrow it in `Layer3`.

In `Layer1` we have two functions from two libraries: one reports failures by throwing exceptions, the other by returning `result<>`:

{{% snippet "using_outcome.cpp" "decl_f_g" %}}  

In `Layer2_old` we cannot use exceptions, so its function `h` uses return type `outcome<>` to report failures. It is using functions `f` and `g` and reports their failures inside `outcome<>`:

{{% snippet "using_outcome.cpp" "def_h" %}}

#1. Operator `TRY` can forward failures encoded in `result<T, EC>` as `outcome<T, EC, EP>` without any loss in information. You can also use `TRY` to forward failure from one `outcome<>` to another.

#2. You can store the current exception through `std::exception_ptr` inside `outcome<T, EC, EP>` without any loss in information
    (provided that `EP` is `std::exception_ptr`).
