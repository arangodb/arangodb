// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/type.hpp>

#include <boost/mpl/size.hpp>
#include <boost/mpl/vector.hpp>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


//! [front]
#include <boost/hana/ext/boost/mpl/vector.hpp> // bridge header

using Vector = mpl::vector<int, char, float>;
static_assert(hana::front(Vector{}) == hana::type_c<int>, "");
//! [front]


namespace _ns0 {
//! [size]
using Size = mpl::size<Vector>::type;
static_assert(hana::equal(Size{}, hana::int_c<3>), ""); // breaks!
//! [size]
}


//! [size-fixed]
#include <boost/hana/ext/boost/mpl/integral_c.hpp>

using Size = mpl::size<Vector>::type;
static_assert(hana::equal(Size{}, hana::int_c<3>), "");
//! [size-fixed]


int main() { }
