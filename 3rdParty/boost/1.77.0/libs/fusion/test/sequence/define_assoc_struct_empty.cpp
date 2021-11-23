/*=============================================================================
    Copyright (c) 2016 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/container.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/sequence/io/out.hpp>
#include <boost/fusion/iterator/equal_to.hpp>
#include <boost/fusion/adapted/struct/define_assoc_struct.hpp>
#include <boost/mpl/assert.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/static_assert.hpp>
#include <iostream>

BOOST_FUSION_DEFINE_ASSOC_STRUCT(BOOST_PP_EMPTY(), empty_struct, )

int
main()
{
    using namespace boost;
    using namespace boost::fusion;

    std::cout << tuple_open('[');
    std::cout << tuple_close(']');
    std::cout << tuple_delimiter(", ");

    {
        BOOST_MPL_ASSERT_NOT((traits::is_view<empty_struct>));
        BOOST_STATIC_ASSERT(!traits::is_view<empty_struct>::value);
        empty_struct e;

        std::cout << e << std::endl;
        BOOST_TEST(e == make_vector());

        BOOST_STATIC_ASSERT(fusion::result_of::size<empty_struct>::value == 0);
        BOOST_STATIC_ASSERT(fusion::result_of::empty<empty_struct>::value);
    }

    {
        vector<> v;
        empty_struct e;
        BOOST_TEST(v == e);
        BOOST_TEST_NOT(v != e);
        BOOST_TEST_NOT(v < e);
        BOOST_TEST(v <= e);
        BOOST_TEST_NOT(v > e);
        BOOST_TEST(v >= e);
    }

    {
        empty_struct e;

        // conversion from empty_struct to vector
        vector<> v(e);
        v = e;

        // conversion from empty_struct to list
        //list<> l(e);
        //l = e;
    }

    { // begin/end
        typedef fusion::result_of::begin<empty_struct>::type b;
        typedef fusion::result_of::end<empty_struct>::type e;

        BOOST_MPL_ASSERT((fusion::result_of::equal_to<b, e>));
    }

    BOOST_MPL_ASSERT((mpl::is_sequence<empty_struct>));
    BOOST_MPL_ASSERT_NOT((fusion::result_of::has_key<empty_struct, void>));
    BOOST_MPL_ASSERT_NOT((fusion::result_of::has_key<empty_struct, int>));

    return boost::report_errors();
}
