/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/adapted/struct/adapt_struct_named.hpp>
#include <boost/fusion/sequence/intrinsic/at.hpp>
#include <boost/fusion/sequence/intrinsic/size.hpp>
#include <boost/fusion/sequence/intrinsic/empty.hpp>
#include <boost/fusion/sequence/intrinsic/front.hpp>
#include <boost/fusion/sequence/intrinsic/back.hpp>
#include <boost/fusion/sequence/intrinsic/value_at.hpp>
#include <boost/fusion/sequence/io/out.hpp>
#include <boost/fusion/container/vector/vector.hpp>
#include <boost/fusion/container/list/list.hpp>
#include <boost/fusion/container/generation/make_vector.hpp>
#include <boost/fusion/container/vector/convert.hpp>
#include <boost/fusion/sequence/comparison/equal_to.hpp>
#include <boost/fusion/sequence/comparison/not_equal_to.hpp>
#include <boost/fusion/sequence/comparison/less.hpp>
#include <boost/fusion/sequence/comparison/less_equal.hpp>
#include <boost/fusion/sequence/comparison/greater.hpp>
#include <boost/fusion/sequence/comparison/greater_equal.hpp>
#include <boost/fusion/mpl.hpp>
#include <boost/fusion/support/is_view.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/assert.hpp>
#include <iostream>
#include <string>

namespace ns
{
    struct point
    {
        int x;
        int y;
        int z;
    };
}

#if BOOST_PP_VARIADICS

    // this creates a fusion view: boost::fusion::adapted::point
    BOOST_FUSION_ADAPT_STRUCT_NAMED(
        ns::point, point,
        x,
        y,
        z
    )

    // this creates a fusion view: ns1::s1
    struct s { int m; };
    BOOST_FUSION_ADAPT_STRUCT_NAMED_NS(s, (ns1), s1, m)

#else // BOOST_PP_VARIADICS

    // this creates a fusion view: boost::fusion::adapted::point
    BOOST_FUSION_ADAPT_STRUCT_NAMED(
        ns::point, point,
        (int, x)
        (BOOST_FUSION_ADAPT_AUTO, y)
        (auto, z)
    )

    // this creates a fusion view: ns1::s1
    struct s { int m; };
    BOOST_FUSION_ADAPT_STRUCT_NAMED_NS(s, (ns1), s1, (auto, m))

#endif 

struct empty_struct {};
BOOST_FUSION_ADAPT_STRUCT_NAMED(empty_struct, renamed_empty_struct, )
BOOST_FUSION_ADAPT_STRUCT_NAMED_NS(empty_struct, (ns1), renamed_empty_struct1, ) 

int
main()
{
    using namespace boost::fusion;
    using namespace boost;

    std::cout << tuple_open('[');
    std::cout << tuple_close(']');
    std::cout << tuple_delimiter(", ");

    {
        BOOST_MPL_ASSERT((traits::is_view<adapted::point>));
        ns::point basep = {123, 456, 789};
        adapted::point p(basep);

        std::cout << at_c<0>(p) << std::endl;
        std::cout << at_c<1>(p) << std::endl;
        std::cout << at_c<2>(p) << std::endl;
        std::cout << p << std::endl;
        BOOST_TEST(p == make_vector(123, 456, 789));

        at_c<0>(p) = 6;
        at_c<1>(p) = 9;
        at_c<2>(p) = 12;
        BOOST_TEST(p == make_vector(6, 9, 12));

        BOOST_STATIC_ASSERT(boost::fusion::result_of::size<adapted::point>::value == 3);
        BOOST_STATIC_ASSERT(!boost::fusion::result_of::empty<adapted::point>::value);

        BOOST_TEST(front(p) == 6);
        BOOST_TEST(back(p) == 12);
    }

    {
        fusion::vector<int, float, int> v1(4, 2.f, 2);
        ns::point p = {5, 3, 3};
        adapted::point v2(p);

        fusion::vector<long, double, int> v3(5, 4., 4);
        BOOST_TEST(v1 < v2);
        BOOST_TEST(v1 <= v2);
        BOOST_TEST(v2 > v1);
        BOOST_TEST(v2 >= v1);
        BOOST_TEST(v2 < v3);
        BOOST_TEST(v2 <= v3);
        BOOST_TEST(v3 > v2);
        BOOST_TEST(v3 >= v2);
    }

    {
        // conversion from adapted::point to vector
        ns::point basep = {5, 3, 3};
        adapted::point p(basep);
        fusion::vector<int, long, int> v(p);
        v = p;
    }

    {
        // conversion from adapted::point to list
        ns::point basep = {5, 3, 3};
        adapted::point p(basep);
        fusion::list<int, long, int> l(p);
        l = p;
    }

    { // begin/end
        using namespace boost::fusion;
        using boost::is_same;

        typedef boost::fusion::result_of::begin<ns1::s1>::type b;
        typedef boost::fusion::result_of::end<ns1::s1>::type e;
        // this fails
        BOOST_MPL_ASSERT((is_same<boost::fusion::result_of::next<b>::type, e>));
    }


    {
        BOOST_MPL_ASSERT((mpl::is_sequence<adapted::point>));
        BOOST_MPL_ASSERT((boost::is_same<
            boost::fusion::result_of::value_at_c<adapted::point,0>::type
          , mpl::front<adapted::point>::type>));
    }

    return boost::report_errors();
}

