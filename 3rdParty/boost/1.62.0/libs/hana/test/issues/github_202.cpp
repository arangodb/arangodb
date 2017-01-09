// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/tuple.hpp>

#include <vector>
namespace hana = boost::hana;


using Vector = std::vector<int>;

static_assert(
    sizeof(hana::tuple<
        hana::pair<hana::int_<0>, Vector>,
        hana::pair<hana::int_<1>, Vector>,
        hana::pair<hana::int_<2>, Vector>,
        hana::pair<hana::int_<3>, Vector>
    >)
    ==
    sizeof(hana::tuple<Vector, Vector, Vector, Vector>)
, "");

int main() { }
