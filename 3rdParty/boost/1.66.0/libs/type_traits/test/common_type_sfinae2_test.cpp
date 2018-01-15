
//  Copyright Peter Dimov 2015
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.tt.org/LICENSE_1_0.txt)

#include "test.hpp"
#include "check_integral_constant.hpp"
#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits/common_type.hpp>
#  include <boost/type_traits/integral_constant.hpp>
#endif
#include <iostream>

typedef char(&s1)[1];
typedef char(&s2)[2];

template<class T> s1 has_type_impl( typename T::type * );
template<class T> s2 has_type_impl( ... );

template<class T> struct has_type: tt::integral_constant<bool, sizeof(has_type_impl<T>(0)) == sizeof(s1)> {};

struct X {};
struct Y {};

TT_TEST_BEGIN(common_type_sfinae2)
{
#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES) && !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

    BOOST_CHECK_INTEGRAL_CONSTANT( (has_type< tt::common_type<int, void*> >::value), false );
    BOOST_CHECK_INTEGRAL_CONSTANT( (has_type< tt::common_type<X, Y> >::value), false );
    BOOST_CHECK_INTEGRAL_CONSTANT( (has_type< tt::common_type<X&, int const*> >::value), false );
    BOOST_CHECK_INTEGRAL_CONSTANT( (has_type< tt::common_type<X, Y, int, void*> >::value), false );

#endif
}
TT_TEST_END
