// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_EXAMPLE_CPPCON_2014_MATRIX_COMPARABLE_HPP
#define BOOST_HANA_EXAMPLE_CPPCON_2014_MATRIX_COMPARABLE_HPP

#include "matrix.hpp"

#include <boost/hana/all.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/eval_if.hpp>
#include <boost/hana/zip_with.hpp>


namespace boost { namespace hana {
    template <unsigned R1, unsigned C1, unsigned R2, unsigned C2>
    struct equal_impl<cppcon::Matrix<R1, C1>, cppcon::Matrix<R2, C2>> {
        template <typename M1, typename M2>
        static constexpr auto apply(M1 const& m1, M2 const& m2) {
            return eval_if(bool_c<R1 == R2 && C1 == C2>,
                [&](auto _) {
                    return all(zip_with(_(equal), cppcon::rows(m1),
                                                  cppcon::rows(m2)));
                },
                [] { return false_c; }
            );
        }
    };
}}

#endif // !BOOST_HANA_EXAMPLE_CPPCON_2014_MATRIX_COMPARABLE_HPP
