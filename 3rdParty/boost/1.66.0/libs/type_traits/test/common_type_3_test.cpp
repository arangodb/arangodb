
//  Copyright Peter Dimov 2015
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.tt.org/LICENSE_1_0.txt)

#include "test.hpp"
#include "check_type.hpp"
#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/common_type.hpp>
#endif
#include <iostream>

TT_TEST_BEGIN(common_type_3)
{
    // just check whether the nullary specialization compiles
    tt::common_type<> tmp;
    (void)tmp;
}
TT_TEST_END
