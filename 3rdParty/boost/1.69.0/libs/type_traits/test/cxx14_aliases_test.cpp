
//  Copyright Peter Dimov 2017
//  Use, modification and distribution are subject to the 
//  Boost Software License, Version 1.0. (See accompanying file 
//  LICENSE_1_0.txt or copy at http://www.tt.org/LICENSE_1_0.txt)

#ifdef TEST_STD
#  include <type_traits>
#else
#  include <boost/type_traits.hpp>
#endif
#include "test.hpp"
#include "check_type.hpp"
#include <iostream>

TT_TEST_BEGIN(cxx14_aliases_test)
{
#if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)

    BOOST_CHECK_TYPE(tt::add_const_t<int>, int const);
    BOOST_CHECK_TYPE(tt::add_cv_t<int>, int const volatile);
    BOOST_CHECK_TYPE(tt::add_lvalue_reference_t<int>, int&);
    BOOST_CHECK_TYPE(tt::add_pointer_t<int>, int*);
    BOOST_CHECK_TYPE(tt::add_rvalue_reference_t<int>, int&&);
    BOOST_CHECK_TYPE(tt::add_volatile_t<int>, int volatile);
    BOOST_CHECK_TYPE3(tt::common_type_t<char, short>, int);
    BOOST_CHECK_TYPE4(tt::conditional_t<true, char, short>, char);
    BOOST_CHECK_TYPE4(tt::conditional_t<false, char, short>, short);
    BOOST_CHECK_TYPE3(tt::copy_cv_t<char, short const volatile>, char const volatile);
    BOOST_CHECK_TYPE(tt::decay_t<char const(&)[7]>, char const*);
    BOOST_CHECK_TYPE(tt::make_signed_t<unsigned char>, signed char);
    BOOST_CHECK_TYPE(tt::make_unsigned_t<signed char>, unsigned char);
    BOOST_CHECK_TYPE(tt::remove_all_extents_t<int[][10][20]>, int);
    BOOST_CHECK_TYPE(tt::remove_const_t<int const>, int);
    BOOST_CHECK_TYPE(tt::remove_cv_t<int const volatile>, int);
    BOOST_CHECK_TYPE(tt::remove_extent_t<int[]>, int);
    BOOST_CHECK_TYPE(tt::remove_pointer_t<int*>, int);
    BOOST_CHECK_TYPE(tt::remove_reference_t<int&>, int);
    BOOST_CHECK_TYPE(tt::remove_volatile_t<int volatile>, int);
    BOOST_CHECK_TYPE(tt::floating_point_promotion_t<float>, double);
    BOOST_CHECK_TYPE(tt::integral_promotion_t<char>, int);
    BOOST_CHECK_TYPE(tt::promote_t<char>, int);

#endif
}
TT_TEST_END
