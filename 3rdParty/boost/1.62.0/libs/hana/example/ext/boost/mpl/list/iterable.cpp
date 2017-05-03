// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/drop_front.hpp>
#include <boost/hana/drop_while.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/boost/mpl/list.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/type.hpp>

#include <boost/mpl/list.hpp>
#include <type_traits>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


BOOST_HANA_CONSTANT_CHECK(hana::front(mpl::list<int, char, void>{}) == hana::type_c<int>);

BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::drop_front(mpl::list<int, char, void>{}),
    mpl::list<char, void>{}
));

BOOST_HANA_CONSTANT_CHECK(hana::equal(
    hana::drop_while(mpl::list<float, double const, int, float&>{},
                     hana::trait<std::is_floating_point>),
    mpl::list<int, float&>{}
));

int main() { }
