
//  (C) Copyright Edward Diener 2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

//------------------------------------------------------------------------------

#include <boost/mpl/assert.hpp>
#include <boost/function_types/property_tags.hpp>

namespace ft = boost::function_types;
namespace mpl = boost::mpl;

typedef ft::tag<ft::variadic,ft::non_volatile,ft::non_const,ft::default_cc> tag1;
typedef ft::tag<ft::const_non_volatile,ft::const_qualified> tag2;
typedef ft::tag<ft::cv_qualified,ft::variadic> tag3;
typedef ft::null_tag tag4;
typedef ft::tag<ft::variadic,ft::volatile_non_const> tag5;
typedef ft::tag<ft::volatile_qualified,ft::non_const,ft::variadic> tag6;
typedef ft::tag<ft::non_variadic,ft::const_non_volatile,ft::default_cc> tag7;
typedef ft::tag<ft::non_cv,ft::default_cc> tag8;
    
BOOST_MPL_ASSERT((ft::has_property_tag<tag1, ft::variadic>));
BOOST_MPL_ASSERT((ft::has_property_tag<tag2, ft::const_qualified>));
BOOST_MPL_ASSERT((ft::has_property_tag<tag3, ft::cv_qualified>));
BOOST_MPL_ASSERT((ft::has_property_tag<tag4, ft::null_tag>));
BOOST_MPL_ASSERT((ft::has_property_tag<tag5, ft::volatile_qualified>));
BOOST_MPL_ASSERT((ft::has_property_tag<tag6, ft::volatile_qualified>));
BOOST_MPL_ASSERT((ft::has_property_tag<tag7, ft::const_qualified>));
BOOST_MPL_ASSERT((ft::has_property_tag<tag8, ft::default_cc>));

BOOST_MPL_ASSERT((ft::has_variadic_property_tag<tag1>));
BOOST_MPL_ASSERT((ft::has_variadic_property_tag<tag3>));
BOOST_MPL_ASSERT((ft::has_variadic_property_tag<tag5>));
BOOST_MPL_ASSERT((ft::has_variadic_property_tag<tag6>));
BOOST_MPL_ASSERT((ft::has_default_cc_property_tag<tag1>));
BOOST_MPL_ASSERT((ft::has_default_cc_property_tag<tag7>));
BOOST_MPL_ASSERT((ft::has_default_cc_property_tag<tag8>));
BOOST_MPL_ASSERT((ft::has_const_property_tag<tag2>));
BOOST_MPL_ASSERT((ft::has_const_property_tag<tag3>));
BOOST_MPL_ASSERT((ft::has_const_property_tag<tag7>));
BOOST_MPL_ASSERT((ft::has_volatile_property_tag<tag3>));
BOOST_MPL_ASSERT((ft::has_volatile_property_tag<tag5>));
BOOST_MPL_ASSERT((ft::has_volatile_property_tag<tag6>));
BOOST_MPL_ASSERT((ft::has_cv_property_tag<tag3>));
BOOST_MPL_ASSERT((ft::has_null_property_tag<tag4>));

BOOST_MPL_ASSERT_NOT((ft::has_property_tag<tag1, ft::const_qualified>));
BOOST_MPL_ASSERT_NOT((ft::has_property_tag<tag2, ft::cv_qualified>));
BOOST_MPL_ASSERT_NOT((ft::has_property_tag<tag3, ft::null_tag>));
BOOST_MPL_ASSERT_NOT((ft::has_property_tag<tag4, ft::volatile_qualified>));
BOOST_MPL_ASSERT_NOT((ft::has_property_tag<tag5, ft::const_qualified>));
BOOST_MPL_ASSERT_NOT((ft::has_property_tag<tag6, ft::default_cc>));
BOOST_MPL_ASSERT_NOT((ft::has_property_tag<tag7, ft::variadic>));
BOOST_MPL_ASSERT_NOT((ft::has_property_tag<tag8, ft::volatile_qualified>));

BOOST_MPL_ASSERT_NOT((ft::has_variadic_property_tag<tag2>));
BOOST_MPL_ASSERT_NOT((ft::has_default_cc_property_tag<tag6>));
BOOST_MPL_ASSERT_NOT((ft::has_const_property_tag<tag4>));
BOOST_MPL_ASSERT_NOT((ft::has_const_property_tag<tag5>));
BOOST_MPL_ASSERT_NOT((ft::has_volatile_property_tag<tag7>));
BOOST_MPL_ASSERT_NOT((ft::has_cv_property_tag<tag1>));
BOOST_MPL_ASSERT_NOT((ft::has_null_property_tag<tag8>));
