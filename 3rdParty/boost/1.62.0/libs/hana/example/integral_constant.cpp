// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/mult.hpp>
#include <boost/hana/negate.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/tuple.hpp>

#include <string>
#include <vector>
namespace hana = boost::hana;


int main() {

{

//! [operators]
BOOST_HANA_CONSTANT_CHECK(hana::int_c<1> + hana::int_c<3> == hana::int_c<4>);

// Mixed-type operations are supported, but only when it involves a
// promotion, and not a conversion that could be lossy.
BOOST_HANA_CONSTANT_CHECK(hana::size_c<3> * hana::ushort_c<5> == hana::size_c<15>);
BOOST_HANA_CONSTANT_CHECK(hana::llong_c<15> == hana::int_c<15>);
//! [operators]

}{

//! [times_loop_unrolling]
std::string s;
for (char c = 'x'; c <= 'z'; ++c)
    hana::int_<5>::times([&] { s += c; });

BOOST_HANA_RUNTIME_CHECK(s == "xxxxxyyyyyzzzzz");
//! [times_loop_unrolling]

}{

//! [times_higher_order]
std::string s;
BOOST_HANA_CONSTEXPR_LAMBDA auto functions = hana::make_tuple(
    [&] { s += "x"; },
    [&] { s += "y"; },
    [&] { s += "z"; }
);
hana::for_each(functions, hana::int_<5>::times);
BOOST_HANA_RUNTIME_CHECK(s == "xxxxxyyyyyzzzzz");
//! [times_higher_order]

}{

//! [from_object]
std::string s;
for (char c = 'x'; c <= 'z'; ++c)
    hana::int_c<5>.times([&] { s += c; });

BOOST_HANA_RUNTIME_CHECK(s == "xxxxxyyyyyzzzzz");
//! [from_object]

}{

//! [times_with_index_runtime]
std::vector<int> v;
hana::int_<5>::times.with_index([&](auto index) { v.push_back(index); });

BOOST_HANA_RUNTIME_CHECK(v == std::vector<int>{0, 1, 2, 3, 4});
//! [times_with_index_runtime]

//! [times_with_index_compile_time]
constexpr auto xs = hana::tuple_c<int, 0, 1, 2>;
hana::int_<3>::times.with_index([xs](auto index) {
    BOOST_HANA_CONSTANT_CHECK(xs[index] == index);
});
//! [times_with_index_compile_time]

}{

//! [literals]
using namespace hana::literals; // contains the _c suffix

BOOST_HANA_CONSTANT_CHECK(1234_c == hana::llong_c<1234>);
BOOST_HANA_CONSTANT_CHECK(-1234_c == hana::llong_c<-1234>);
BOOST_HANA_CONSTANT_CHECK(1_c + (3_c * 4_c) == hana::llong_c<1 + (3 * 4)>);
//! [literals]

}{

//! [integral_c]
BOOST_HANA_CONSTANT_CHECK(hana::integral_c<int, 2> == hana::int_c<2>);
static_assert(decltype(hana::integral_c<int, 2>)::value == 2, "");
//! [integral_c]

}

}
