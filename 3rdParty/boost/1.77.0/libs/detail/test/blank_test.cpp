
// Copyright 2018 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/blank.hpp>
#include <boost/static_assert.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/type_traits/is_pod.hpp>
#include <boost/type_traits/is_empty.hpp>
#include <boost/type_traits/is_stateless.hpp>
#if !defined(BOOST_NO_IOSTREAM)
#include <sstream>
#endif

int main()
{
    BOOST_STATIC_ASSERT((boost::is_pod<boost::blank>::value));
    BOOST_STATIC_ASSERT((boost::is_empty<boost::blank>::value));
    BOOST_STATIC_ASSERT((boost::is_stateless<boost::blank>::value));

    boost::blank b1,b2;
    BOOST_TEST(b1 == b2);
    BOOST_TEST(b1 <= b2);
    BOOST_TEST(b1 >= b2);
    BOOST_TEST(!(b1 != b2));
    BOOST_TEST(!(b1 < b2));
    BOOST_TEST(!(b1 > b2));

#if !defined(BOOST_NO_IOSTREAM)
    std::stringstream s;
    s << "(" << b1 << ")";
    BOOST_TEST(s.str() == "()");
#endif

    return boost::report_errors();
}
