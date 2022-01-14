// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/mpl/integral_c.hpp>

#include <boost/hana/tuple.hpp>
#include <boost/hana/value.hpp>

#include <laws/constant.hpp>

#include <boost/mpl/int.hpp>
#include <boost/mpl/integral_c.hpp>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


int main() {
    // value
    static_assert(hana::value(mpl::integral_c<int, 0>{}) == 0, "");
    static_assert(hana::value(mpl::integral_c<int, 1>{}) == 1, "");
    static_assert(hana::value(mpl::integral_c<int, 3>{}) == 3, "");

    // laws
    hana::test::TestConstant<hana::ext::boost::mpl::integral_c_tag<int>>{
        hana::make_tuple(
            mpl::int_<-10>{}, mpl::int_<-2>{}, mpl::integral_c<int, 0>{},
            mpl::integral_c<int, 1>{}, mpl::integral_c<int, 3>{}
        ),
        hana::tuple_t<int, long, long long>
    };
}
