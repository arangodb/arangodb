// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/boost/mpl/vector.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/fold_left.hpp>
#include <boost/hana/if.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/type.hpp>

#include <boost/mpl/vector.hpp>
#include <type_traits>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


auto types = mpl::vector<long, float, short, float, long, long double>{};
auto number_of_floats = hana::fold_left(types, hana::int_c<0>, [](auto count, auto t) {
    return hana::if_(hana::trait<std::is_floating_point>(t),
        count + hana::int_c<1>,
        count
    );
});

BOOST_HANA_CONSTANT_CHECK(number_of_floats == hana::int_c<3>);

int main() { }
