+++
title = "`value_or_none<T>`"
description = "A boolean concept matching types with an optional value."
+++

If on C++ 20 or the Concepts TS is enabled, a boolean concept matching types with a public `.has_value()` observer which returns `bool`, and a public `.value()` observer function.

If without Concepts, a static constexpr bool which is true for types matching the same requirements, using a SFINAE based emulation.

This concept matches optional-like types such as {{% api "std::optional<T>" %}}. Note it also matches {{% api "std::expected<T, E>" %}}, which also has an optional-like interface. You may thus wish to preferentially match {{% api "concepts::value_or_error<T, E>" %}} for any given `T`.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::concepts`

*Header*: `<boost/outcome/convert.hpp>`

*Legacy*: This was named `convert::ValueOrNone<T>` in Outcome v2.1 and earlier. Define {{% api "BOOST_OUTCOME_ENABLE_LEGACY_SUPPORT_FOR" %}} to less than `220` to enable.
