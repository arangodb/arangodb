// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_EXAMPLE_CPPCON_2014_MATRIX_GROUP_HPP
#define BOOST_HANA_EXAMPLE_CPPCON_2014_MATRIX_GROUP_HPP

#include "matrix.hpp"

#include <boost/hana/concept/group.hpp>

#include <utility>


namespace boost { namespace hana {
    template <unsigned R, unsigned C>
    struct minus_impl<cppcon::Matrix<R, C>, cppcon::Matrix<R, C>> {
        template <typename M1, typename M2>
        static constexpr decltype(auto) apply(M1&& m1, M2&& m2) {
            return element_wise(minus)(
                std::forward<M1>(m1),
                std::forward<M2>(m2)
            );
        }
    };
}}

#endif // !BOOST_HANA_EXAMPLE_CPPCON_2014_MATRIX_GROUP_HPP
