// Boost.Units - A C++ library for zero-overhead dimensional analysis and 
// unit/quantity manipulation and conversion
//
// Copyright (C) 2003-2008 Matthias Christian Schabel
// Copyright (C) 2008 Steven Watanabe
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/** 
\file
    
\brief test_units_1.cpp

\details
Test unit class.

Output:
@verbatim
@endverbatim
**/

#include "test_header.hpp"

#include <boost/units/pow.hpp>

namespace bu = boost::units;

int test_main(int,char *[])
{
    BOOST_CONSTEXPR_OR_CONST bu::dimensionless D;
    
    BOOST_CONSTEXPR_OR_CONST bu::length        L;
    BOOST_CONSTEXPR_OR_CONST bu::mass          M;
    BOOST_CONSTEXPR_OR_CONST bu::time          T;
    
    BOOST_CHECK(+L == L);
    BOOST_CHECK(-L == L);
    BOOST_CHECK(L+L == L);
    BOOST_CHECK(L-L == L);

    BOOST_CHECK(+M == M);
    BOOST_CHECK(-M == M);
    BOOST_CHECK(M+M == M);
    BOOST_CHECK(M-M == M);
    
    BOOST_CONSTEXPR_OR_CONST bu::area          A;
    BOOST_CONSTEXPR_OR_CONST bu::energy        E;
    BOOST_CONSTEXPR_OR_CONST bu::velocity      V;
    
    BOOST_CHECK(L*L == A);
    BOOST_CHECK(A == L*L);
    
    BOOST_CHECK(L/L == D);
    BOOST_CHECK(D == L/L);
    
    BOOST_CHECK(L/T == V);
    BOOST_CHECK(V == L/T);
    
    BOOST_CHECK(M*L*L/T/T == E);
    BOOST_CHECK(M*(L/T)*(L/T) == E);
    BOOST_CHECK(M*bu::pow<2>(L/T) == E);
    BOOST_CHECK(bu::root<2>(E/M) == V);
    
    return 0;
}
