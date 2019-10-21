+++
title = "`ValueOrError<T, E>`"
description = "A boolean concept matching types with either a value or an error."
+++

If on C++ 20 or the Concepts TS is enabled, a boolean concept matching types with a public `.has_value()` observer which returns `bool`, a public `.value()` observer function, and a public `.error()` observer function.

If without Concepts, a static constexpr bool which is true for types matching the same requirements, using a SFINAE based emulation.

This concept matches expected-like types such as {{% api "std::expected<T, E>" %}}, one of which is {{% api "basic_result<T, E, NoValuePolicy>" %}}. Be aware it does not differentiate between value-or-error types and value-and-error types if they present the interface matched above.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::convert`

*Header*: `<boost/outcome/convert.hpp>`