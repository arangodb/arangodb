// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/sum.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::sum<>(hana::make_range(hana::int_c<1>, hana::int_c<6>)) == hana::int_c<15>);

static_assert(hana::sum<>(hana::make_tuple(1, hana::int_c<3>, hana::long_c<-5>, 9)) == 8, "");

static_assert(hana::sum<unsigned long>(hana::make_tuple(1ul, 3ul)) == 4ul, "");

int main() { }
