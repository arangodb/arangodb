+++
title = "`is_error_type_enum<E, Enum>`"
description = "A customisable integral constant type true for `E` types constructible from `Enum` types which are to receive error throwing no-value policies."
+++

A customisable integral constant type true for `E` types constructible from `Enum` types which are to receive error throwing no-value policies

*Overridable*: By template specialisation into the `trait` namespace.

*Default*: False. Specialisations exist for:

- `<boost/outcome/boost_result.hpp>`
    - `boost::system::error_code` to `boost::system::is_error_condition_enum<Enum>::value`.

- `<boost/outcome/std_result.hpp>`
    - `std::error_code` to `std::is_error_condition_enum<Enum>::value`.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::trait`

*Header*: `<boost/outcome/trait.hpp>`