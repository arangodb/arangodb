// Boost.Units - A C++ library for zero-overhead dimensional analysis and 
// unit/quantity manipulation and conversion
//
// Copyright (C) 2014 Erik Erlandson
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//#include <boost/units/systems/information.hpp>

/** 
\file

\brief information.cpp

\details
Demonstrate information unit system.

Output:
@verbatim
bytes= 1.25e+08 B
bits= 8e+06 b
nats= 4605.17 nat
1024 bytes in a kibi-byte
8.38861e+06 bits in a mebi-byte
0.000434294 hartleys in a milli-nat
entropy in bits= 1 b
entropy in nats= 0.693147 nat
entropy in hartleys= 0.30103 Hart
entropy in shannons= 1 Sh
entropy in bytes= 0.125 B
@endverbatim
**/

#include <cmath>
#include <iostream>
using std::cout;
using std::endl;
using std::log;

#include <boost/units/quantity.hpp>
#include <boost/units/io.hpp>
#include <boost/units/conversion.hpp>
namespace bu = boost::units;
using bu::quantity;
using bu::conversion_factor;

// SI prefixes
#include <boost/units/systems/si/prefixes.hpp>
namespace si = boost::units::si;

// information unit system
#include <boost/units/systems/information.hpp>
using namespace bu::information;

// Define a function for the entropy of a bernoulli trial.
// The formula is computed using natural log, so the units are in nats.
// The user provides the desired return unit, the only restriction being that it
// must be a unit of information.  Conversion to the requested return unit is 
// accomplished automatically by the boost::units library.
template <typename Sys>
constexpr
quantity<bu::unit<bu::information_dimension, Sys> > 
bernoulli_entropy(double p, const bu::unit<bu::information_dimension, Sys>&) {
    typedef bu::unit<bu::information_dimension, Sys> requested_unit;
    return quantity<requested_unit>((-(p*log(p) + (1-p)*log(1-p)))*nats);
}

int main(int argc, char** argv) {
    // a quantity of information (default in units of bytes) 
    quantity<info> nbytes(1 * si::giga * bit);
    cout << "bytes= " << nbytes << endl;

    // a quantity of information, stored as bits
    quantity<hu::bit::info> nbits(1 * si::mega * byte);
    cout << "bits= " << nbits << endl;

    // a quantity of information, stored as nats
    quantity<hu::nat::info> nnats(2 * si::kilo * hartleys);
    cout << "nats= " << nnats << endl;

    // how many bytes are in a kibi-byte?
    cout << conversion_factor(kibi * byte, byte) << " bytes in a kibi-byte" << endl;

    // how many bits are in a mebi-byte?
    cout << conversion_factor(mebi * byte, bit) << " bits in a mebi-byte" << endl;

    // how many hartleys are in a milli-nat?
    cout << conversion_factor(si::milli * nat, hartley) << " hartleys in a milli-nat" << endl;

    // compute the entropy of a fair coin flip, in various units of information:
    cout << "entropy in bits= " << bernoulli_entropy(0.5, bits) << endl;
    cout << "entropy in nats= " << bernoulli_entropy(0.5, nats) << endl;
    cout << "entropy in hartleys= " << bernoulli_entropy(0.5, hartleys) << endl;
    cout << "entropy in shannons= " << bernoulli_entropy(0.5, shannons) << endl;
    cout << "entropy in bytes= " << bernoulli_entropy(0.5, bytes) << endl;

    return 0;
}
