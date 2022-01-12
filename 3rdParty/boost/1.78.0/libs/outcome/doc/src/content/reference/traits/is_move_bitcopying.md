+++
title = "`is_move_bitcopying<T>`"
description = "(>= Outcome v2.2.0) A customisable integral constant type true for `T` types which are move bitcopying safe."
+++

A customisable integral constant type true for `T` types which are move bitcopying
safe. As per [P1029 move bitcopying](https://wg21.link/P1029), these are types for
which:

1. There is an inline, constexpr-available, default constructor.
2. The move constructor has side effects equivalent to `memcpy` of source to destination,
followed by a `memcpy` of a default constructed instance to source.
3. That the destruction of a default constructed instance has no visible
side effects.

This implies that if you move from a bit copying type, you need not call its
destructor, even if that is a virtual destructor.

If you opt your types into this trait, Outcome will track moved-from state and
not call the destructor for your type on moved-from instances. Obviously enough
this is, in current C++ standards, undefined behaviour. However it very
significantly improves the quality of codegen during inlining.

*Overridable*: By template specialisation into the `trait` namespace.

*Default*: False. Default specialisations exist for:

- `<boost/outcome/experimental/status_result.hpp>`
    - True for `BOOST_OUTCOME_SYSTEM_ERROR2_NAMESPACE::status_code<DomainType>` if trait
    `BOOST_OUTCOME_SYSTEM_ERROR2_NAMESPACE::traits::is_move_bitcopying<BOOST_OUTCOME_SYSTEM_ERROR2_NAMESPACE::status_code<DomainType>>::value`
    is true.
    - True for `BOOST_OUTCOME_SYSTEM_ERROR2_NAMESPACE::errored_status_code<DomainType>` if trait
    `BOOST_OUTCOME_SYSTEM_ERROR2_NAMESPACE::traits::is_move_bitcopying<BOOST_OUTCOME_SYSTEM_ERROR2_NAMESPACE::errored_status_code<DomainType>>::value`
    is true.

*Namespace*: `BOOST_OUTCOME_V2_NAMESPACE::trait`

*Header*: `<boost/outcome/trait.hpp>`