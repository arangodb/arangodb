/*
   Copyright (c) Marshall Clow 2013.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <boost/config.hpp>
#include <boost/algorithm/cxx17/for_each_n.hpp>

#include "iterator_test.hpp"

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

namespace ba = boost::algorithm;

struct for_each_test
{
    for_each_test() {}
    static int count;
    void operator()(int& i) {++i; ++count;}
};

int for_each_test::count = 0;

void test_for_each_n ()
{
    typedef input_iterator<int*> Iter;
    int ia[] = {0, 1, 2, 3, 4, 5};
    const unsigned s = sizeof(ia)/sizeof(ia[0]);

    {
	for_each_test::count = 0;
    Iter it = ba::for_each_n(Iter(ia), 0, for_each_test());
    BOOST_CHECK(it == Iter(ia));
    BOOST_CHECK(for_each_test::count == 0);
    }

    {
	for_each_test::count = 0;
    Iter it = ba::for_each_n(Iter(ia), s, for_each_test());

    BOOST_CHECK(it == Iter(ia+s));
    BOOST_CHECK(for_each_test::count == s);
    for (unsigned i = 0; i < s; ++i)
        BOOST_CHECK(ia[i] == static_cast<int>(i+1));
    }

    {
	for_each_test::count = 0;
    Iter it = ba::for_each_n(Iter(ia), 1, for_each_test());

    BOOST_CHECK(it == Iter(ia+1));
    BOOST_CHECK(for_each_test::count == 1);
    for (unsigned i = 0; i < 1; ++i)
        BOOST_CHECK(ia[i] == static_cast<int>(i+2));
    }
}

BOOST_AUTO_TEST_CASE( test_main )
{
  test_for_each_n ();
}
