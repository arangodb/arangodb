/*
 [auto_generated]
 libs/numeric/odeint/test/split.cpp

 [begin_description]
 Test the range split.
 [end_description]

 Copyright 2013 Karsten Ahnert
 Copyright 2013 Mario Mulansky
 Copyright 2013 Pascal Germroth

 Distributed under the Boost Software License, Version 1.0.
 (See accompanying file LICENSE_1_0.txt or
 copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#define BOOST_TEST_MODULE odeint_split

#include <iostream>

#include <boost/test/unit_test.hpp>
#include <boost/numeric/odeint/util/split_adaptor.hpp>
#include <boost/range/irange.hpp>

template<class T>
inline void dump_range(const T &r) {
    std::cout << '[';
    std::copy(boost::begin(r), boost::end(r), std::ostream_iterator<
        typename std::iterator_traits<
            typename boost::range_iterator<const T>::type
        >::value_type >(std::cout, " ") );
    std::cout << ']';
}

template<class A, class B>
inline void check_equal_range(const A a, const B b) {
    BOOST_CHECK_EQUAL_COLLECTIONS( boost::begin(a), boost::end(a), boost::begin(b), boost::end(b) );
}


using namespace boost::unit_test;
using namespace boost::numeric::odeint::detail;
using namespace boost;

BOOST_AUTO_TEST_CASE( test_eleven )
{
    // 0 1 2 3 | 4 5 6 7 | 8 9 10 11
    check_equal_range( irange(0, 12) | split(0, 3), irange(0, 4) );
    check_equal_range( irange(0, 12) | split(1, 3), irange(4, 8) );
    check_equal_range( irange(0, 12) | split(2, 3), irange(8, 12) );
}

BOOST_AUTO_TEST_CASE( test_ten )
{
    // 0 1 2 3 | 4 5 6 7 | 8 9 10
    check_equal_range( irange(0, 11) | split(0, 3), irange(0, 4) );
    check_equal_range( irange(0, 11) | split(1, 3), irange(4, 8) );
    check_equal_range( irange(0, 11) | split(2, 3), irange(8, 11) );
}

BOOST_AUTO_TEST_CASE( test_nine )
{
    // 0 1 2 3 | 4 5 6 | 7 8 9
    check_equal_range( irange(0, 10) | split(0, 3), irange(0, 4) );
    check_equal_range( irange(0, 10) | split(1, 3), irange(4, 7) );
    check_equal_range( irange(0, 10) | split(2, 3), irange(7, 10) );
}
