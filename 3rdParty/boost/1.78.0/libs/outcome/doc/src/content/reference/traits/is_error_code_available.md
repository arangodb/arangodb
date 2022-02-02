+++
title = "`is_error_code_available<T>`"
description = "True if an error code can be constructed from a `T`."
+++

`::value` is true if an error code can be constructed from a `T` e.g. if there exists an ADL discovered free function `make_error_code(T)`.
`::type` is the type that would result if `::value` is true, else `void`.

*Overridable*: By template specialisation into the `trait` namespace.

*Default*: True if `T` is an error code, else to metaprogramming which performs the ADL discovery of `make_error_code(T)`. Note that the STL defines multiple overloads of an ADL discovered free function {{% api "std::make_error_code(T)" %}} for its error enumerations, as does Boost.System for the Boost error enumerations. Thus this trait will pick up those free functions for those error types.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::trait`

*Header*: `<boost/outcome/trait.hpp>`

*Variable alias*: `is_error_code_available_v<T>`
