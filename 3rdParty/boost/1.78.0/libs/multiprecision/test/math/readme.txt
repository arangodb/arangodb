These tests verify that the multiprecision types can interoperate with Boost.Math.

We use Boost.Math's own test data for these and test at various precisions which trigger
different code inside the Math lib.  We don't test at very high precision here as
Boost.Math's test data doesn't go much beyond 35 digits (for good reason, some compilers
choke when attempting to parse higher precision numbers as double's).


//  Copyright 2018 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

