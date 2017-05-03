/*=============================================================================
    Copyright (c) 1999-2003 Jaakko Jarvi
    Copyright (c) 2001-2011 Joel de Guzman
    Copyright (c) 2006 Dan Marsden

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/container/map/map.hpp>
#include <boost/fusion/container/generation/make_map.hpp>
#include <boost/fusion/container/generation/map_tie.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/sequence/intrinsic/at.hpp>
#include <boost/fusion/mpl.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/mpl/insert_range.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/begin.hpp>
#include <boost/mpl/equal.hpp>
#include <boost/static_assert.hpp>
#include <string>

struct k1 {};
struct k2 {};
struct k3 {};
struct k4 {};

namespace test_detail
{
    // classes with different kinds of conversions
    class AA {};
    class BB : public AA {};
    struct CC { CC() {} CC(const BB&) {} };
    struct DD { operator CC() const { return CC(); }; };
}

boost::fusion::map<
    boost::fusion::pair<k1, double>,
    boost::fusion::pair<k2, double>,
    boost::fusion::pair<k3, double>,
    boost::fusion::pair<k4, double>
>
foo(int i)
{
   return boost::fusion::make_map<k1, k2, k3, k4>(i, i+1, i+2, i+3);
}

void
test()
{
    using namespace boost::fusion;
    using namespace test_detail;

    map<pair<k1, int>, pair<k2, char> > t1(4, 'a');
    map<pair<k1, int>, pair<k2, char> > t2(5, 'b');
    t2 = t1;
    BOOST_TEST(at_c<0>(t1).second == at_c<0>(t2).second);
    BOOST_TEST(at_c<1>(t1).second == at_c<1>(t2).second);

    map<pair<k1, int>, pair<k2, char const*> > t4(4, "a");
    map<pair<k1, long>, pair<k2, std::string> > t3(2, std::string("a"));
    t3 = t4;
    BOOST_TEST((double)at_c<0>(t4).second == at_c<0>(t3).second);
    BOOST_TEST(at_c<1>(t4).second == at_c<1>(t3).second);

    // testing copy and assignment with implicit conversions
    // between elements testing tie

    map<pair<k1, char>, pair<k2, BB*>, pair<k3, BB>, pair<k4, DD> > t;
    map<pair<k1, int>, pair<k2, AA*>, pair<k3, CC>, pair<k4, CC> > a(t);
    a = t;

    int i; char c; double d;
    map_tie<k1, k2, k3>(i, c, d) = make_map<k1, k2, k3>(1, 'a', 5.5);

    BOOST_TEST(i==1);
    BOOST_TEST(c=='a');
    BOOST_TEST(d>5.4 && d<5.6);

    // returning a map with conversion
    foo(2);
}

int
main()
{
    test();
    return boost::report_errors();
}

