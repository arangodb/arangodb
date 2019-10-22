
//  Copyright Peter Dimov 2015
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.tt.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/common_type.hpp>
#endif
#include "test.hpp"
#include "check_type.hpp"
#include <iostream>

struct X {};
struct Y {};

TT_TEST_BEGIN(common_type_sfinae)
{
#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES) && !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

    {
        tt::common_type<int, void*> tmp;
        (void)tmp;
    }

    {
        tt::common_type<X, Y> tmp;
        (void)tmp;
    }

    {
        tt::common_type<X&, int const*> tmp;
        (void)tmp;
    }

    {
        tt::common_type<X, Y, int, void*> tmp;
        (void)tmp;
    }

#endif
}
TT_TEST_END
