+++
title = "TRY operations"
description = ""
weight = 30
tags = ["try"]
+++

In the implementation of function `print_half` we have seen the usage of the macro {{< api "BOOST_OUTCOME_TRYV(expr)/BOOST_OUTCOME_TRY(expr)" >}}:

```c++
BOOST_OUTCOME_TRY (auto i, BigInt::fromString(text));
```

The `BOOST_OUTCOME_TRY` macro uses C macro overloading to select between two implementations based on the number of
input parameters. If there is exactly one input parameter i.e. without the `i`, the control statement is
roughly equivalent to:

```c++
auto __result = BigInt::fromString(text);
if (!__result)
  return __result.as_failure();
```

Where `__result` is a compile time generated unique name. This single argument form is equivalent to
`BOOST_OUTCOME_TRYV(expr)`, incidentally.

If there are between two and eight parameters, this control statement is roughly equivalent to:

```c++
auto __result = BigInt::fromString(text);
if (!__result)
  return __result.as_failure();
auto i = __result.value();
```

So here `i` as the first C macro parameter is set to the value of any successful result. 

C macro overloads are provided for up to eight arguments. To prevent the
confounding of the C preprocessor by commas in template specifications causing more than
eight arguments appearing to the C preprocessor, you should consider wrapping the
second argument in brackets.

If you are within a C++ Coroutine, you ought to use {{< api "BOOST_OUTCOME_CO_TRYV(expr)/BOOST_OUTCOME_CO_TRY(expr)" >}}
instead.

<hr>

### Compiler-specific extension: `BOOST_OUTCOME_TRYX`

{{% notice note %}}
This macro makes use of a proprietary extension in GCC and clang, and is not
portable. The macro is not made available on unsupported compilers,
so you can test for its presence using `#ifdef BOOST_OUTCOME_TRYX`.
{{% /notice %}}

GCC and Clang provide an extension to C++ known as
[statement expressions](https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html "GCC docs on statement expressions").
These make it possible to use a more convenient macro: `BOOST_OUTCOME_TRYX`, which is an expression. With the above macro, the above declaration of variable `i` can be rewritten to:

```c++
int i = BOOST_OUTCOME_TRYX (BigInt::fromString(text));
```

This has an advantage that you can use it any place where you can put an expression, e.g., in "simple initialization":

```c++
if (int i = BOOST_OUTCOME_TRYX(BigInt::fromString(text)))
  use_a_non_zero_int(i);
```

or in as a subexpression of a bigger full expression:

```c++
int ans = BOOST_OUTCOME_TRYX(BigInt::fromString("1")) + BOOST_OUTCOME_TRYX(BigInt::fromString("2"));
```

There is also an {{< api "BOOST_OUTCOME_CO_TRYX(expr)" >}} if you are inside a C++ Coroutine.
