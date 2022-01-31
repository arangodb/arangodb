+++
title = "Inspecting result<T, EC>"
description = ""
weight = 20
tags = ["nodiscard", "value", "error", "try"]
+++

Suppose we will be writing a function `print_half` that takes a `std::string` representing an integer and prints half the integer:

{{% snippet "using_result.cpp" "half_decl" %}}

The type `result<void>` means that there is no value to be returned upon success, but that the operation might still fail, and we may be interested in inspecting the cause of the failure. The class template `result<>` is declared with the attribute `[[nodiscard]]`, which means the compiler will warn you if you forget to inspect the returned object (in C++ 17 or later).

The implementation will do the following: if the integral number can be represented by an `int`, we will convert to `int` and use its arithmetical operations. If the number is too large, we will fall back to using a custom `BigInt` implementation that needs to allocate memory. In the implementation we will use the function `convert` defined in the previous section.

{{% snippet "using_result.cpp" "half_impl" %}}

#1. You test if `result<>` object represents a successful operation with contextual conversion to `bool`.

#2. The function `.value()` extracts the successfully returned `int`.

#3. The function `.error()` allows you to inspect the error sub-object, representing information about the reason for failure.

#4. Macro `BOOST_OUTCOME_TRY` represents a control statement. It implies that the expression in the second argument returns a `result<>`. The function is defined as:

{{% snippet "using_result.cpp" "from_string" %}}

   Our control statement means: if `fromString` returned failure, this same error information should be returned from `print_half`, even though the type of `result<>` is different. If `fromString` returned success, we create  variable `i` of type `BigInt` with the value returned from `fromString`. If control goes to subsequent line, it means `fromString` succeeded and variable of type `BigInt` is in scope.

#5. In the return statement we extract the error information and use it to initialize the return value from `print_half`. We could have written `return r.error();` instead,
    and it would have the same effect, but `r.as_failure()` will work when implicit construction from `E` has been disabled due to `T` and `E` having a constructibility relationship.

#6. Function `success()` returns an object of type `success<void>` representing success. This is implicitly converted by
all `result` and `outcome` types into a successful return, default constructing any `T` if necessary.
