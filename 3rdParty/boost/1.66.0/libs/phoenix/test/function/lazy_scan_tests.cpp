////////////////////////////////////////////////////////////////////////////
// lazy_scan_tests.cpp
//
// tests on scanl scanr map zip and zip_with
//
////////////////////////////////////////////////////////////////////////////
/*=============================================================================
    Copyright (c) 2000-2003 Brian McNamara and Yannis Smaragdakis
    Copyright (c) 2001-2007 Joel de Guzman
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix/core/limits.hpp>

#include <boost/detail/lightweight_test.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/function/lazy_prelude.hpp>


int main()
{
    namespace phx = boost::phoenix;
    using boost::phoenix::arg_names::arg1;
    using boost::phoenix::arg_names::arg2;
    using boost::phoenix::arg_names::arg3;
    using namespace phx;

    list<int> l = enum_from(2);
    list<int> evens = filter(even,l);
    list<int> odds  = filter(odd,l);
    list<int> even4 = take(4,evens)();
    list<int> odd4  = take(4,odds)();

    BOOST_TEST(head(scanr(plus,0,even4))() == 20);
    BOOST_TEST(last(scanl(plus,0,even4))() == 20);
    BOOST_TEST(head(scanr1(plus,even4))()  == 20);
    BOOST_TEST(last(scanl1(plus,even4))()  == 20);

    list<int> map_result = map(inc,even4)();
    list<int> zip_with_result = zip_with(plus,even4,odd4)();

    BOOST_TEST(foldl1(plus,map_result)()  == 24);
    BOOST_TEST(last(zip_with_result)()    == 17);

    typedef std::pair<int,int> pair_ii;
    list<pair_ii> zip_result1 = zip_with(make_pair,even4,odd4)();
    list<pair_ii> zip_result2 = zip(even4,odd4)();

    BOOST_TEST(head(zip_result1)().first   == 2);
    BOOST_TEST(last(zip_result2)().second  == 9);

    return boost::report_errors();
}
