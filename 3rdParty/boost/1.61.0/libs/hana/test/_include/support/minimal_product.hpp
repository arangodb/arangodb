// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef TEST_SUPPORT_MINIMAL_PRODUCT_HPP
#define TEST_SUPPORT_MINIMAL_PRODUCT_HPP

#include <boost/hana/fwd/core/make.hpp>
#include <boost/hana/fwd/first.hpp>
#include <boost/hana/fwd/second.hpp>

#include <type_traits>


struct MinimalProduct;

template <typename X, typename Y>
struct product_t {
    X fst;
    Y snd;
    using hana_tag = MinimalProduct;
};

struct make_minimal_product {
    template <typename T, typename U>
    constexpr product_t<typename std::decay<T>::type,
                        typename std::decay<U>::type>
    operator()(T&& t, U&& u) const {
        return {static_cast<T&&>(t), static_cast<U&&>(u)};
    }
};

constexpr make_minimal_product minimal_product{};

namespace boost { namespace hana {
    //////////////////////////////////////////////////////////////////////////
    // Product
    //////////////////////////////////////////////////////////////////////////
    template <>
    struct make_impl<MinimalProduct> {
        template <typename X, typename Y>
        static constexpr auto apply(X x, Y y)
        { return ::minimal_product(x, y); }
    };

    template <>
    struct first_impl<MinimalProduct> {
        template <typename P>
        static constexpr decltype(auto) apply(P&& p)
        { return p.fst; }
    };

    template <>
    struct second_impl<MinimalProduct> {
        template <typename P>
        static constexpr decltype(auto) apply(P&& p)
        { return p.snd; }
    };
}} // end namespace boost::hana

#endif // !TEST_SUPPORT_MINIMAL_PRODUCT_HPP
