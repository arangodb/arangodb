// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef TEST_SUPPORT_EQUIVALENCE_CLASS_HPP
#define TEST_SUPPORT_EQUIVALENCE_CLASS_HPP

#include <boost/hana/fwd/equal.hpp>


struct EquivalenceClass { };

template <typename Token, typename T>
struct equivalence_class_impl {
    Token equivalence_class;
    T unwrap;
    using hana_tag = EquivalenceClass;
};

template <typename Token, typename X>
constexpr equivalence_class_impl<Token, X> equivalence_class(Token token, X x) {
    return {token, x};
}

namespace boost { namespace hana {
    template <>
    struct equal_impl<EquivalenceClass, EquivalenceClass> {
        template <typename X, typename Y>
        static constexpr auto apply(X x, Y y)
        { return hana::equal(x.equivalence_class, y.equivalence_class); }
    };
}} // end namespace boost::hana

#endif // !TEST_SUPPORT_EQUIVALENCE_CLASS_HPP
