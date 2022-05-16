+++
title = "`value_or_error<T, U>`"
description = "A customisable converter of `value_or_error<T, E>` concept matching types."
+++

A customisable converter of {{% api "concepts::value_or_error<T, E>" %}} concept matching types. It must have the following form:

```c++
// `T` will be the destination basic_result or basic_outcome.
// `U` will be the decayed form of the `value_or_error<T, E>` concept matching input type.
template <class T> struct value_or_error<T, U>
{
  // False to indicate that this converter wants `basic_result`/`basic_outcome` to reject all other `basic_result`
  static constexpr bool enable_result_inputs = false;
  // False to indicate that this converter wants `basic_outcome` to reject all other `basic_outcome`
  static constexpr bool enable_outcome_inputs = false;
  
  // `X` will be the raw input form of `U`. It must return a `T`.
  template<class X> constexpr T operator()(X &&v);
};
```

*Overridable*: By template specialisation into the `convert` namespace.

*Default*: If decayed `X` is same as `U`, concept `value_or_error<U>` matches, `X::value_type` is `void` or is explicitly constructible to `T::value_type`, and `X::error_type` is `void` or is explicitly constructible to `T::error_type`, then `operator()(X &&)` is made available.

`operator()(X &&v)` tests if `v.has_value()` is true, if so then a `T` with successful value is returned, else a `T` with unsuccessful value. If the input type was `void`, a default constructed value is used for either, else a move/copy construction from the source is performed.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::convert`

*Header*: `<boost/outcome/convert.hpp>`
