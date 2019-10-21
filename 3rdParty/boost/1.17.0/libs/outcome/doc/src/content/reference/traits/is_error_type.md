+++
title = "`is_error_type<E>`"
description = "A customisable integral constant type true for `E` types which are to receive error throwing no-value policies."
+++

A customisable integral constant type true for `E` types which are to receive error throwing no-value policies.

*Overridable*: By template specialisation into the `trait` namespace.

*Default*: False. Specialisations to true exist for:

- `<boost/outcome/boost_result.hpp>`
    - `boost::system::error_code`
    - `boost::system::errc::errc_t`
    - `boost::exception_ptr`

- `<boost/outcome/std_result.hpp>`
    - `std::error_code`
    - `std::errc`
    - `std::exception_ptr`

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::trait`

*Header*: `<boost/outcome/trait.hpp>`