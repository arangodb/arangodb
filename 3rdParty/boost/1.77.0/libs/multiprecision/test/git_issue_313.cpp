///////////////////////////////////////////////////////////////////////////////
//  Copyright 2021 Christopher Kormanyos. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iomanip>
#include <sstream>

#include <boost/multiprecision/cpp_dec_float.hpp>

#include "test.hpp"

void test()
{
   using local_decfloat_type = boost::multiprecision::cpp_dec_float_100;

   bool result0_is_ok = false;
   bool result1_is_ok = false;

   {
      const local_decfloat_type test = pow(local_decfloat_type{2}, 30) * pow(local_decfloat_type{10}, -3);

      std::stringstream strm;

      strm << std::setprecision(std::numeric_limits<local_decfloat_type>::digits10)
           << std::fixed
           << test;

      result0_is_ok =
         (strm.str() == "1073741.8240000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
   }

   {
      const local_decfloat_type test(0.625F);

      std::stringstream strm;

      strm << std::setprecision(std::numeric_limits<local_decfloat_type>::digits10)
           << std::fixed
           << test;

      result1_is_ok =
         (strm.str() == "0.6250000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");
   }

   const bool result_is_ok = (result0_is_ok && result1_is_ok);

   BOOST_CHECK_EQUAL(result_is_ok, true);
}

int main()
{
   test();

   return boost::report_errors();
}
