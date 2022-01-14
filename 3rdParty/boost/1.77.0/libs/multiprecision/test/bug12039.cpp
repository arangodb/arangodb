///////////////////////////////////////////////////////////////////////////////
//  Copyright 2016 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/cpp_bin_float.hpp>

int main()
{
   typedef boost::multiprecision::number<boost::multiprecision::backends::cpp_bin_float<256> >  ext_float_t;
   typedef boost::multiprecision::number<boost::multiprecision::backends::cpp_bin_float<2046> > long_ext_float_t;

   ext_float_t x = 5e15;
   x += 0.5;
   ext_float_t x1 = x + 255.0 / (1 << 20); // + 2^-12 - eps
   ext_float_t x2 = x + 257.0 / (1 << 20); // + 2^-12 + eps
   double      d1 = x1.convert_to<double>();
   double      d2 = x2.convert_to<double>();

   std::cout << std::setprecision(18) << d1 << std::endl;
   std::cout << std::setprecision(18) << d2 << std::endl;

   x        = 1e7 + 0.5;
   x1       = x + ldexp(255.0, -38); // + 2^-30 - eps
   x2       = x + ldexp(257.0, -38); // + 2^-30 + eps
   float f1 = x1.convert_to<float>();
   float f2 = x2.convert_to<float>();

   std::cout << std::setprecision(9) << f1 << std::endl;
   std::cout << std::setprecision(9) << f2 << std::endl;

   long_ext_float_t lf(1);
   lf += std::numeric_limits<long_ext_float_t>::epsilon();
   lf += std::numeric_limits<float>::epsilon() / 2;
   BOOST_ASSERT(lf != 1);
   float f3 = lf.convert_to<float>();
   std::cout << std::setprecision(9) << f3 << std::endl;

   return (d1 == d2) && (f1 == f2) && (f3 != 1) ? 0 : 1;
}
