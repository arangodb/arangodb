#include <boost/config.hpp>

//
//  bind_type_test.cpp
//
//  Copyright (c) 2015 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/bind.hpp>
#include <boost/detail/lightweight_test.hpp>

//

template<int I> struct X
{
};

void fv1( X<1> )
{
}

void fv2( X<1>, X<2> )
{
}

void fv3( X<1>, X<2>, X<3> )
{
}

void fv4( X<1>, X<2>, X<3>, X<4> )
{
}

void fv5( X<1>, X<2>, X<3>, X<4>, X<5> )
{
}

void fv6( X<1>, X<2>, X<3>, X<4>, X<5>, X<6> )
{
}

void fv7( X<1>, X<2>, X<3>, X<4>, X<5>, X<6>, X<7> )
{
}

void fv8( X<1>, X<2>, X<3>, X<4>, X<5>, X<6>, X<7>, X<8> )
{
}

void fv9( X<1>, X<2>, X<3>, X<4>, X<5>, X<6>, X<7>, X<8>, X<9> )
{
}

void test()
{
    boost::bind( fv1, _1 )( X<1>() );
    boost::bind( fv2, _1, _2 )( X<1>(), X<2>() );
    boost::bind( fv3, _1, _2, _3 )( X<1>(), X<2>(), X<3>() );
    boost::bind( fv4, _1, _2, _3, _4 )( X<1>(), X<2>(), X<3>(), X<4>() );
    boost::bind( fv5, _1, _2, _3, _4, _5 )( X<1>(), X<2>(), X<3>(), X<4>(), X<5>() );
    boost::bind( fv6, _1, _2, _3, _4, _5, _6 )( X<1>(), X<2>(), X<3>(), X<4>(), X<5>(), X<6>() );
    boost::bind( fv7, _1, _2, _3, _4, _5, _6, _7 )( X<1>(), X<2>(), X<3>(), X<4>(), X<5>(), X<6>(), X<7>() );
    boost::bind( fv8, _1, _2, _3, _4, _5, _6, _7, _8 )( X<1>(), X<2>(), X<3>(), X<4>(), X<5>(), X<6>(), X<7>(), X<8>() );
    boost::bind( fv9, _1, _2, _3, _4, _5, _6, _7, _8, _9 )( X<1>(), X<2>(), X<3>(), X<4>(), X<5>(), X<6>(), X<7>(), X<8>(), X<9>() );
}

int main()
{
    test();
    return boost::report_errors();
}
