// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/eval_if.hpp>
#include <boost/hana/lazy.hpp>
#include <boost/hana/traits.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


// eval_if with heterogeneous branches and a Constant condition
BOOST_HANA_CONSTEXPR_LAMBDA auto safe_make_unsigned = [](auto t) {
    return hana::eval_if(hana::traits::is_integral(t),
        hana::make_lazy(hana::traits::make_unsigned)(t),
        hana::make_lazy(t)
    );
};

BOOST_HANA_CONSTANT_CHECK(safe_make_unsigned(hana::type_c<void>) == hana::type_c<void>);
BOOST_HANA_CONSTANT_CHECK(safe_make_unsigned(hana::type_c<int>) == hana::type_c<unsigned int>);


// eval_if with homogeneous branches and a constexpr or runtime condition
BOOST_HANA_CONSTEXPR_LAMBDA auto safe_divide = [](auto x, auto y) {
    return hana::eval_if(y == 0,
        [=](auto) { return 0; },
        [=](auto _) { return _(x) / y; }
    );
};

int main() {
    BOOST_HANA_CONSTEXPR_CHECK(safe_divide(6, 3) == 2);
    BOOST_HANA_CONSTEXPR_CHECK(safe_divide(6, 0) == 0);
}
