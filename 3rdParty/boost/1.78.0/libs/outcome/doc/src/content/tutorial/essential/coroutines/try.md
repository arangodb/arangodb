+++
title = "Coroutine TRY operation"
weight = 10
tags = [ "coroutines", "try" ]
+++

As one cannot call statement `return` from within a Coroutine, the very first part of Outcome's
support for Coroutines is {{% api "BOOST_OUTCOME_CO_TRYV(expr)/BOOST_OUTCOME_CO_TRY(expr)" %}},
which is literally the same as `BOOST_OUTCOME_TRY()` except that `co_return` is called
to return early instead of `return`.

```c++
eager<result<std::string>> to_string(int x)
{
  if(x >= 0)
  {
    BOOST_OUTCOME_CO_TRY(convert(x));
  }
  co_return "out of range";
}
```
