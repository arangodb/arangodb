+++
title = "`is_exception_ptr_available<T>`"
description = "True if an exception ptr can be constructed from a `T`."
+++

`::value` is true if an exception ptr can be constructed from a `T` e.g. if there exists an ADL discovered free function `make_exception_ptr(T)`.
`::type` is the type that would result if `::value` is true, else `void`.

*Overridable*: By template specialisation into the `trait` namespace.

*Default*: True if `T` is an exception ptr, else to metaprogramming which performs the ADL discovery of `make_exception_ptr(T)`. Note that the STL defines an ADL discovered free function {{% api "std::make_exception_ptr(T)" %}}. Thus this trait will pick up that free function.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::trait`

*Header*: `<boost/outcome/trait.hpp>`

*Variable alias*: `is_exception_ptr_available_v<T>`
