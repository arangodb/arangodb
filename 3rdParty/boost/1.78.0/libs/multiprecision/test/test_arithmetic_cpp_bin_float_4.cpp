///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#include <boost/multiprecision/cpp_bin_float.hpp>

#include "libs/multiprecision/test/test_arithmetic.hpp"

using namespace boost::multiprecision;

typedef number<cpp_bin_float<500>, et_on> cpp_bin_float_500_et_s;

template <>
struct related_type<cpp_bin_float_500_et_s>
{
   typedef number<cpp_bin_float<500, digit_base_10, std::allocator<char> >, et_on> type;
};

int main()
{
   test<cpp_bin_float_500_et_s>();
   return boost::report_errors();
}
