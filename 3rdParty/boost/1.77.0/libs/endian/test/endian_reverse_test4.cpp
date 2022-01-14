// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/endian/conversion.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <cstddef>

template<class T> void test_reverse_inplace( T x )
{
    using boost::endian::endian_reverse_inplace;

    T x2( x );

    endian_reverse_inplace( x2 );
    endian_reverse_inplace( x2 );
    BOOST_TEST( x == x2 );

    boost::endian::native_to_little_inplace( x2 );
    boost::endian::little_to_native_inplace( x2 );
    BOOST_TEST( x == x2 );

    boost::endian::native_to_big_inplace( x2 );
    boost::endian::big_to_native_inplace( x2 );
    BOOST_TEST( x == x2 );
}

struct X
{
    int v1_;
    int v2_;
};

inline bool operator==( X const& x1, X const& x2 )
{
    return x1.v1_ == x2.v1_ && x1.v2_ == x2.v2_;
}

inline void endian_reverse_inplace( X & x )
{
    using boost::endian::endian_reverse_inplace;

    endian_reverse_inplace( x.v1_ );
    endian_reverse_inplace( x.v2_ );
}

struct Y
{
    X x1_;
    X x2_[ 2 ];
};

inline bool operator==( Y const& y1, Y const& y2 )
{
    return y1.x1_ == y2.x1_ && y1.x2_[0] == y2.x2_[0] && y1.x2_[1] == y2.x2_[1];
}

inline void endian_reverse_inplace( Y & y )
{
    using boost::endian::endian_reverse_inplace;

    endian_reverse_inplace( y.x1_ );
    endian_reverse_inplace( y.x2_ );
}

int main()
{
    Y y = { { 1, 2 }, { { 3, 4 }, { 5, 6 } } };

    test_reverse_inplace( y );

    return boost::report_errors();
}
