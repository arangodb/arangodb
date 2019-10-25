//////////////////////////////////////////////////////////////////
// example94.cpp
//
// Copyright (c) 2015 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// illustrate usage of safe<int> as drop-in replacement for int in
// a more complex library.  Use an example from the boost.rational
// library with modifications to use safe<int> rather than int

//  rational number example program  ----------------------------------------//

//  (C) Copyright Paul Moore 1999. Permission to copy, use, modify, sell
//  and distribute this software is granted provided this copyright notice
//  appears in all copies. This software is provided "as is" without express or
//  implied warranty, and with no claim as to its suitability for any purpose.

// boostinspect:nolicense (don't complain about the lack of a Boost license)
// (Paul Moore hasn't been in contact for years, so there's no way to change the
// license.)

//  Revision History
//  14 Dec 99  Initial version

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <boost/config.hpp>
#include <limits>
#include <exception>
#include <boost/rational.hpp>

#include <boost/safe_numerics/safe_integer.hpp>

using std::cout;
using std::endl;
using boost::rational;
using namespace boost::safe_numerics;

using int_type = safe<int>;

int main ()
{
    rational<int_type> half(1,2);
    rational<int_type> one(1);
    rational<int_type> two(2);

    // Some basic checks
    assert(half.numerator() == 1);
    assert(half.denominator() == 2);
//    assert(boost::rational_cast<double>(half) == 0.5);

    static_assert(
        ! boost::safe_numerics::is_safe<rational<int_type>>::value,
        "rational<int_type> is safe"
    );

    // Arithmetic
    assert(half + half == one);
    assert(one - half == half);
    assert(two * half == one);
    assert(one / half == two);

    // With conversions to integer
    assert(half+half == 1);
    assert(2 * half == one);
    assert(2 * half == 1);
    assert(one / half == 2);
    assert(1 / half == 2);

    // Sign handling
    rational<int_type> minus_half(-1,2);
    assert(-half == minus_half);
    assert(abs(minus_half) == half);

    // Do we avoid overflow?
    int maxint = (std::numeric_limits<int>::max)();
    rational<int_type> big(maxint, 2);
    assert(2 * big == maxint);

    // Print some of the above results
    cout << half << "+" << half << "=" << one << endl;
    cout << one << "-" << half << "=" << half << endl;
    cout << two << "*" << half << "=" << one << endl;
    cout << one << "/" << half << "=" << two << endl;
    cout << "abs(" << minus_half << ")=" << half << endl;
    cout << "2 * " << big << "=" << maxint
         << " (rational: " << rational<int>(maxint) << ")" << endl;

    // Some extras
//    rational<int_type> pi(22,7);
//    cout << "pi = " << boost::rational_cast<double>(pi) << " (nearly)" << endl;

    // Exception handling
    try {
        rational<int_type> r;        // Forgot to initialise - set to 0
        r = 1/r;                // Boom!
    }
    catch (const boost::bad_rational &e) {
        cout << "Bad rational, as expected: " << e.what() << endl;
    }
    catch (...) {
        cout << "Wrong exception raised!" << endl;
    }
    return 0;
}
