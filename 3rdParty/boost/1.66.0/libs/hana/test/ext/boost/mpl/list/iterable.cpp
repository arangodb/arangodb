// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/mpl/list.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/drop_front_exactly.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <laws/iterable.hpp>

#include <boost/mpl/list.hpp>
namespace hana = boost::hana;
namespace mpl = boost::mpl;


struct t1; struct t2; struct t3; struct t4;

int main() {
    // front
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::front(mpl::list<t1>{}),
            hana::type_c<t1>
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::front(mpl::list<t1, t2>{}),
            hana::type_c<t1>
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::front(mpl::list<t1, t2, t3>{}),
            hana::type_c<t1>
        ));
    }

    // drop_front_exactly
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front_exactly(mpl::list<t1>{}),
            mpl::list<>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front_exactly(mpl::list<t1, t2>{}),
            mpl::list<t2>{}
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front_exactly(mpl::list<t1, t2, t3>{}),
            mpl::list<t2, t3>{}
        ));


        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front_exactly(mpl::list<t1, t2, t3>{}, hana::size_c<2>),
            mpl::list<t3>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front_exactly(mpl::list<t1, t2, t3, t4>{}, hana::size_c<2>),
            mpl::list<t3, t4>{}
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::drop_front_exactly(mpl::list<t1, t2, t3, t4>{}, hana::size_c<3>),
            mpl::list<t4>{}
        ));
    }

    // is_empty
    {
        BOOST_HANA_CONSTANT_CHECK(hana::is_empty(mpl::list<>{}));
        BOOST_HANA_CONSTANT_CHECK(hana::is_empty(mpl::list0<>{}));

        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(mpl::list<t1>{})));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(mpl::list1<t1>{})));

        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(mpl::list<t1, t2>{})));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(mpl::list2<t1, t2>{})));
    }

    // laws
    auto lists = hana::make_tuple(
          mpl::list<>{}
        , mpl::list<t1>{}
        , mpl::list<t1, t2>{}
        , mpl::list<t1, t2, t3>{}
        , mpl::list<t1, t2, t3, t4>{}
    );
    hana::test::TestIterable<hana::ext::boost::mpl::list_tag>{lists};
}
