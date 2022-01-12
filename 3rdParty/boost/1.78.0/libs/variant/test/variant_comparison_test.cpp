//-----------------------------------------------------------------------------
// boost-libs variant/test/variant_comparison_test.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2003 Eric Friedman, Itay Maman
// Copyright (c) 2014-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "boost/variant/variant.hpp"
#include "boost/core/lightweight_test.hpp"

#include <iostream>
#include <sstream>
#include <string>

#include <algorithm>
#include <vector>

#include "boost/detail/workaround.hpp"
#if BOOST_WORKAROUND(BOOST_BORLANDC, BOOST_TESTED_AT(0x0551))
#    pragma warn -lvc // temporary used for parameter warning
#endif

template <typename T>
void assert_equality_comparable(
      const T& x, const T& y, const T& z
    )
{
    // identity check
    BOOST_TEST( !(&x == &y) || (x == y) );
    BOOST_TEST( !(&x == &z) || (x == z) );
    BOOST_TEST( !(&y == &z) || (y == z) );
    BOOST_TEST( !(&x == &y) || !(x != y) );
    BOOST_TEST( !(&x == &z) || !(x != z) );
    BOOST_TEST( !(&y == &z) || !(y != z) );

    // reflexivity check
    BOOST_TEST( (x == x) );
    BOOST_TEST( (y == y) );
    BOOST_TEST( (z == z) );

    // symmetry check
    BOOST_TEST( !(x == y) || (y == x) );
    BOOST_TEST( !(y == x) || (x == y) );
    BOOST_TEST( (x != y) || (y == x) );
    BOOST_TEST( (y != x) || (x == y) );

    BOOST_TEST( !(x == z) || (z == x) );
    BOOST_TEST( !(z == x) || (x == z) );
    BOOST_TEST( (x != z) || (z == x) );
    BOOST_TEST( (z != x) || (x == z) );

    BOOST_TEST( !(y == z) || (z == y) );
    BOOST_TEST( !(z == y) || (y == z) );
    BOOST_TEST( (y != z) || (z == y) );
    BOOST_TEST( (z != y) || (y == z) );

    // transitivity check
    BOOST_TEST( !(x == y && y == z) || (x == z) );
    BOOST_TEST( !(x == z && z == y) || (x == y) );
    BOOST_TEST( !(y == z && z == x) || (y == x) );
}

template <typename T>
void assert_less_than_comparable(
      const T& x, const T& y, const T& z
    )
{
    // irreflexivity check
    BOOST_TEST( !(x < x) );
    BOOST_TEST( !(y < y) );
    BOOST_TEST( !(z < z) );
    BOOST_TEST( !(x > x) );
    BOOST_TEST( !(y > y) );
    BOOST_TEST( !(z > z) );

    BOOST_TEST( (x <= x) );
    BOOST_TEST( (y <= y) );
    BOOST_TEST( (z <= z) );
    BOOST_TEST( (x >= x) );
    BOOST_TEST( (y >= y) );
    BOOST_TEST( (z >= z) );

    // transitivity check
    BOOST_TEST( (x < y) );
    BOOST_TEST( (y < z) );
    BOOST_TEST( (x < z) );

    BOOST_TEST( (x <= y) );
    BOOST_TEST( (y <= z) );
    BOOST_TEST( (x <= z) );

    BOOST_TEST( (z > y) );
    BOOST_TEST( (y > x) );
    BOOST_TEST( (z > x) );

    BOOST_TEST( (z >= y) );
    BOOST_TEST( (y >= x) );
    BOOST_TEST( (z >= x) );

    // antisymmetry check
    BOOST_TEST( !(y < x) );
    BOOST_TEST( !(z < y) );
    BOOST_TEST( !(z < x) );
}

template <typename It>
std::string print_range(It first, It last)
{
    std::ostringstream result;

    while (first != last)
    {
        result << *first << ' ';
        ++first;
    }

    return result.str();
}

int main()
{
    typedef boost::variant<int, std::string> var_t;
    
    var_t var1(3);
    var_t var2(5);
    var_t var3("goodbye");
    var_t var4("hello");

    assert_equality_comparable(var1, var1, var1);
    assert_equality_comparable(var_t(var1), var_t(var1), var_t(var1));
    assert_equality_comparable(var1, var2, var3);

    assert_less_than_comparable(var1, var2, var3);
    assert_less_than_comparable(var2, var3, var4);

    std::vector<var_t> vec;
    vec.push_back( var3 );
    vec.push_back( var2 );
    vec.push_back( var4 );
    vec.push_back( var1 );
    std::sort(vec.begin(), vec.end());

    std::string sort_result( print_range(vec.begin(), vec.end()) );
    BOOST_TEST( sort_result == "3 5 goodbye hello " );

    // https://svn.boost.org/trac/boost/ticket/11751
    int a = 0, b = 0;

    boost::variant< int& > v (a), u (b);
    BOOST_TEST(v == u);

    return boost::report_errors();
}
