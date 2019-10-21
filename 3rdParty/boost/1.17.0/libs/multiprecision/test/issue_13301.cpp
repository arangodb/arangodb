///////////////////////////////////////////////////////////////////////////////
//  Copyright 2016 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


#include <boost/multiprecision/cpp_bin_float.hpp>

int main()
{
   typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<8, boost::multiprecision::backends::digit_base_2> > quarter_float;

   quarter_float qf(256);

   unsigned int ui = qf.convert_to<unsigned int>();
   if (ui != 256)
      return 1;

   boost::uintmax_t m(1), n;
   m <<= std::numeric_limits<boost::uintmax_t>::digits - 1;
   qf = m;
   n = qf.convert_to<boost::uintmax_t>();
   if (m != n)
      return 2;
   qf *= 2;
   n = qf.convert_to<boost::uintmax_t>();
   m = (std::numeric_limits<boost::uintmax_t>::max)();
   if (m != n)
      return 3;

   qf = 256;
   int si = qf.convert_to<int>();
   if (si != 256)
      return 4;
   boost::intmax_t sm(1), sn;
   sm <<= std::numeric_limits<boost::intmax_t>::digits - 1;
   qf = sm;
   sn = qf.convert_to<boost::intmax_t>();
   if (sm != sn)
      return 5;
   qf *= 2;
   sn = qf.convert_to<boost::intmax_t>();
   sm = (std::numeric_limits<boost::intmax_t>::max)();
   if (sm != sn)
      return 6;

   // Again with negative numbers:
   qf = -256;
   si = qf.convert_to<int>();
   if (si != -256)
      return 7;
   sm = 1;
   sm <<= std::numeric_limits<boost::intmax_t>::digits - 1;
   sm = -sm;
   qf = sm;
   sn = qf.convert_to<boost::intmax_t>();
   if (sm != sn)
      return 8;
   qf *= 2;
   sn = qf.convert_to<boost::intmax_t>();
   sm = (std::numeric_limits<boost::intmax_t>::min)();
   if (sm != sn)
      return 9;

   // Now try conversion to cpp_int:
   qf = 256;
   boost::multiprecision::cpp_int i = qf.convert_to<boost::multiprecision::cpp_int>(), j;
   if (i != 256)
      return 10;
   qf = ldexp(qf, 126);
   i = qf.convert_to<boost::multiprecision::cpp_int>();
   j = 256;
   j <<= 126;
   if (i != j)
      return 11;

   return 0;
}

