// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/boost/mpl/list.hpp>
#include <boost/hana/find.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/type.hpp>

#include <boost/mpl/list.hpp>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


BOOST_HANA_CONSTANT_CHECK(
    hana::find_if(mpl::list<int, float, char const*>{}, hana::equal.to(hana::type_c<float>))
    ==
    hana::just(hana::type_c<float>)
);

BOOST_HANA_CONSTANT_CHECK(
    hana::find(mpl::list<int, float, char const*>{}, hana::type_c<void>)
    ==
    hana::nothing
);

int main() { }
