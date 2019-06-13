// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/mpl/integral_c.hpp>

#include <boost/hana/tuple.hpp>

#include <laws/comparable.hpp>
#include <laws/hashable.hpp>

#include <boost/mpl/int.hpp>
#include <boost/mpl/integral_c.hpp>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


int main() {
    auto ints = hana::make_tuple(
        mpl::int_<-10>{}, mpl::int_<-2>{}, mpl::integral_c<int, 0>{},
        mpl::integral_c<int, 1>{}, mpl::integral_c<int, 3>{}
    );

    hana::test::TestComparable<hana::ext::boost::mpl::integral_c_tag<int>>{ints};
    hana::test::TestHashable<hana::ext::boost::mpl::integral_c_tag<void>>{ints};
}
