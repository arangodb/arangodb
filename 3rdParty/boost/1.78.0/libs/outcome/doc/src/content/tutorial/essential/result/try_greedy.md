+++
title = "TRY is greedy"
description = ""
weight = 40
tags = ["try"]
+++

{{< api "BOOST_OUTCOME_TRYV(expr)/BOOST_OUTCOME_TRY(expr)" >}} has 'greedier' implicit conversion semantics than
{{< api "basic_result<T, E, NoValuePolicy>" >}}. For example, this code won't compile:

```c++
outcome::result<int, std::error_code> test(outcome::result<int, std::errc> r)
{
    return r;  // you need to use explicit construction here
    // i.e. return outcome::result<int>(r);
}
```

This is chosen because there is a non-trivial conversion between `std::errc` and `std::error_code`,
so even though that conversion is implicit for `std::error_code`, Outcome does not expose the
implicitness here in order to keep the implicit constructor count low (implicit constructors
add significantly to build times).

The `TRY` operation is more greedy though:

```c++
outcome::result<int, std::error_code> test(outcome::result<int, std::errc> r)
{
    BOOST_OUTCOME_TRY(r);    // no explicit conversion needed
    return r.value();
}
```

This is because `result<int, std::error_code>` will implicitly construct from anything which
either `int` or `std::error_code` will implicitly construct from. However,
`result<int, std::error_code>` will not implicitly construct from `result<int, std::errc>`.

Thus bear this in mind during usage: `TRY` is greedier for implicit conversions than the Outcome
types themselves.
