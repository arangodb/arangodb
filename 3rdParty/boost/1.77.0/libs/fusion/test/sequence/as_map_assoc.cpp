/*=============================================================================
    Copyright (c) 2001-2011 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/adapted/struct/adapt_assoc_struct.hpp>
#include <boost/fusion/container/vector/vector.hpp>
#include <boost/fusion/adapted/mpl.hpp>
#include <boost/fusion/container/generation/make_vector.hpp>
#include <boost/fusion/container/map/convert.hpp>
#include <boost/fusion/container/generation/make_set.hpp>
#include <boost/fusion/container/vector/convert.hpp>
#include <boost/fusion/sequence/comparison/equal_to.hpp>
#include <boost/fusion/sequence/intrinsic/at_key.hpp>
#include <boost/fusion/sequence/io/out.hpp>
#include <boost/fusion/support/pair.hpp>

namespace ns
{
    struct x_member;
    struct y_member;
    struct z_member;

    struct point
    {
        int x;
        int y;
    };
}

BOOST_FUSION_ADAPT_ASSOC_STRUCT(
    ns::point,
    (int, x, ns::x_member)
    (int, y, ns::y_member)
)

int
main()
{
    using namespace boost::fusion;
    using namespace boost;

    std::cout << tuple_open('[');
    std::cout << tuple_close(']');
    std::cout << tuple_delimiter(", ");

    {
        ns::point p = {123, 456};
        std::cout << as_map(p) << std::endl;
    }

    {
        ns::point p = {123, 456};
        boost::fusion::result_of::as_map<ns::point>::type map(p);
        std::cout << at_key<ns::x_member>(map) << std::endl;
        std::cout << at_key<ns::y_member>(map) << std::endl;
        BOOST_TEST(at_key<ns::x_member>(map) == 123);
        BOOST_TEST(at_key<ns::y_member>(map) == 456);
    }

    {
        boost::fusion::result_of::as_map<set<int, char> >::type map(make_set(1, '2'));
        BOOST_TEST(at_key<int>(map) == 1);
        BOOST_TEST(at_key<char>(map) == '2');
    }
    
    {
        // test conversion
        typedef map<
            pair<ns::x_member, int>
          , pair<ns::y_member, int> >
        map_type;

        ns::point p = {123, 456};
        map_type m(p);
        BOOST_TEST(as_vector(m) == make_vector(make_pair<ns::x_member>(123), make_pair<ns::y_member>(456)));
        m = (make_vector(make_pair<ns::x_member>(123), make_pair<ns::y_member>(456))); // test assign
        BOOST_TEST(as_vector(m) == make_vector(make_pair<ns::x_member>(123), make_pair<ns::y_member>(456)));
    }

    return boost::report_errors();
}

