/*=============================================================================
    Copyright (c) 1999-2003 Jaakko Jarvi
    Copyright (c) 2001-2011 Joel de Guzman
    Copyright (c) 2006 Dan Marsden

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/container/map/map.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/sequence/intrinsic/at.hpp>

struct k1 {};
struct k2 {};
struct k3 {};
struct k4 {};

namespace test_detail
{
    // no public default constructor
    class foo
    {
    public:

        explicit foo(int v) : val(v) {}

        bool operator==(const foo& other) const
        {
            return val == other.val;
        }

    private:

        foo() {}
        int val;
    };
}

void
test()
{
    using namespace boost::fusion;
    using namespace test_detail;

    map<
        pair<k1, int>,
        pair<k1, float>,
        pair<k1, bool>,
        pair<k1, foo>
    > t1(5, 12.2f, true, foo(4));

    at_c<0>(t1).second = 6;
    at_c<1>(t1).second = 2.2f;
    at_c<2>(t1).second = false;
    at_c<3>(t1).second = foo(5);

    BOOST_TEST(at_c<0>(t1).second == 6);
    BOOST_TEST(at_c<1>(t1).second > 2.1f && at_c<1>(t1).second < 2.3f);
    BOOST_TEST(at_c<2>(t1).second == false);
    BOOST_TEST(at_c<3>(t1).second == foo(5));
}

int
main()
{
    test();
    return boost::report_errors();
}

