///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/concepts/mp_number_archetypes.hpp>

#include "test_arithmetic.hpp"

int main()
{
   //
   // Orininal Mingw32 has issues with long double that break this, mingw64 is fine (and supported):
   //
#if !(defined(CI_SUPPRESS_KNOWN_ISSUES) && defined(__MINGW32__) && !defined(_WIN64) && BOOST_WORKAROUND(BOOST_GCC, <= 50300))
   test<boost::multiprecision::number<boost::multiprecision::concepts::number_backend_float_architype> >();
#endif
   return boost::report_errors();
}
