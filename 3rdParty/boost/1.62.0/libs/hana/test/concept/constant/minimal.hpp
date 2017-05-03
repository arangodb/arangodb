// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef TEST_CONCEPT_CONSTANT_MINIMAL_HPP
#define TEST_CONCEPT_CONSTANT_MINIMAL_HPP

#include <boost/hana/concept/constant.hpp>
#include <boost/hana/core/when.hpp>
#include <boost/hana/fwd/core/to.hpp>
#include <boost/hana/value.hpp>


template <typename T>
struct minimal_constant_tag {
    using value_type = T;
};

template <typename T, T v>
struct minimal_constant {
    using hana_tag = minimal_constant_tag<T>;
    static constexpr T value_ = v;
};

namespace boost { namespace hana {
    template <typename T>
    struct value_impl<minimal_constant_tag<T>> {
        template <typename N>
        static constexpr T apply() { return N::value_; }
    };

    template <typename T, typename C>
    struct to_impl<minimal_constant_tag<T>, C, hana::when<
        hana::Constant<C>::value &&
        hana::is_convertible<typename C::value_type, T>::value
    >>
        : hana::embedding<hana::is_embedded<typename C::value_type, T>::value>
    {
        template <typename N>
        static constexpr auto apply(N const&)
        { return minimal_constant<T, hana::value<N>()>{}; }
    };
}} // end namespace boost::hana

#endif // !TEST_CONCEPT_CONSTANT_MINIMAL_HPP
