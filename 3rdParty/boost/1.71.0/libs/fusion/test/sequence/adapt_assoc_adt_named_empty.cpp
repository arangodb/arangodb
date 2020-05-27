/*=============================================================================
    Copyright (c) 2016 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/adapted/adt/adapt_assoc_adt_named.hpp>
#include <boost/fusion/sequence/intrinsic/size.hpp>
#include <boost/fusion/sequence/intrinsic/empty.hpp>
#include <boost/fusion/sequence/intrinsic/begin.hpp>
#include <boost/fusion/sequence/intrinsic/end.hpp>
#include <boost/fusion/sequence/io/out.hpp>
#include <boost/fusion/iterator/equal_to.hpp>
#include <boost/fusion/container/vector/vector.hpp>
#include <boost/fusion/container/list/list.hpp>
#include <boost/fusion/container/generation/make_vector.hpp>
#include <boost/fusion/sequence/comparison/equal_to.hpp>
#include <boost/fusion/sequence/comparison/not_equal_to.hpp>
#include <boost/fusion/sequence/comparison/less.hpp>
#include <boost/fusion/sequence/comparison/less_equal.hpp>
#include <boost/fusion/sequence/comparison/greater.hpp>
#include <boost/fusion/sequence/comparison/greater_equal.hpp>
#include <boost/fusion/mpl.hpp>
#include <boost/fusion/support/is_view.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/assert.hpp>
#include <iostream>

class empty_adt{};
BOOST_FUSION_ADAPT_ASSOC_ADT_NAMED(::empty_adt,empty_adt,)

int
main()
{
    using namespace boost::fusion;
    using namespace boost;

    std::cout << tuple_open('[');
    std::cout << tuple_close(']');
    std::cout << tuple_delimiter(", ");

    empty_adt empty;
    {
        BOOST_MPL_ASSERT((traits::is_view<adapted::empty_adt>));
        BOOST_STATIC_ASSERT(traits::is_view<adapted::empty_adt>::value);
        adapted::empty_adt e(empty);

        std::cout << e << std::endl;
        BOOST_TEST(e == make_vector());

        BOOST_STATIC_ASSERT(fusion::result_of::size<adapted::empty_adt>::value == 0);
        BOOST_MPL_ASSERT((fusion::result_of::empty<adapted::empty_adt>));

        BOOST_MPL_ASSERT((fusion::result_of::equal_to<
            fusion::result_of::begin<adapted::empty_adt>::type,
            fusion::result_of::end<adapted::empty_adt>::type>));
    }

    {
        fusion::vector<> v;
        adapted::empty_adt e(empty);
        BOOST_TEST(v == e);
        BOOST_TEST_NOT(e != v);
        BOOST_TEST_NOT(v < e);
        BOOST_TEST(v <= e);
        BOOST_TEST_NOT(e > v);
        BOOST_TEST(e >= v);
    }

    {
        adapted::empty_adt e(empty);

        // conversion from empty_adt to vector
        fusion::vector<> v(e);
        v = e;

        // FIXME
        // conversion from empty_adt to list
        //fusion::list<> l(e);
        //l = e;
    }

    BOOST_MPL_ASSERT((mpl::is_sequence<adapted::empty_adt>));
    BOOST_MPL_ASSERT_NOT((fusion::result_of::has_key<adapted::empty_adt, void>));
    BOOST_MPL_ASSERT_NOT((fusion::result_of::has_key<adapted::empty_adt, int>));

    return boost::report_errors();
}

