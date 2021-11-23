///////////////////////////////////////////////////////////////////////////////
//  Copyright 2019 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/multiprecision/cpp_dec_float.hpp>
#include "test.hpp"

int main()
{
   try{
      std::locale::global(std::locale("en-US"));
      boost::multiprecision::cpp_dec_float_50 d("1234.56");
      std::string s = d.str();

      BOOST_CHECK_EQUAL(s, "1234.56");
   }
   catch(const std::runtime_error&){}  // No en-US locale

   return boost::report_errors();
}


