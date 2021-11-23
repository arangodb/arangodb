//  (C) Copyright Raffi Enficiaud 2019.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

#define BOOST_TEST_MODULE basic_smoke_test4
#include <boost/test/included/unit_test.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/mpl/list.hpp>

template <class U, class V>
struct my_struct {
    typedef typename boost::is_same<U, V>::type type;
};

typedef boost::mpl::list<
    my_struct<int, int>,
    my_struct<int, float>,
    my_struct<float, float>,
    my_struct<char, float>
    > test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(test, T, test_types)
{
    BOOST_TEST((T::type::value));
}

BOOST_AUTO_TEST_SUITE(some_suite)

typedef boost::mpl::list<
    my_struct<float, int>,
    my_struct<int, float>,
    my_struct<float, float>,
    my_struct<char, char>
    > test_types2;

BOOST_AUTO_TEST_CASE_TEMPLATE(test, T, test_types2)
{
    BOOST_TEST((T::type::value));
}

BOOST_AUTO_TEST_SUITE_END();