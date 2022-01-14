/*
*
* Copyright (c) 2018
* John Maddock
*
* Use, modification and distribution are subject to the
* Boost Software License, Version 1.0. (See accompanying file
* LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
*
*/


#ifdef _MSC_VER
#pragma warning(disable:4820 4668)
#endif

#ifdef __APPLE_CC__
#pragma clang diagnostic ignored "-Wc++11-long-long"
#endif

#include <boost/regex.hpp>

void test_proc()
{
   std::string text, re;
   boost::regex exp(re);
   regex_match(text, exp);
}

