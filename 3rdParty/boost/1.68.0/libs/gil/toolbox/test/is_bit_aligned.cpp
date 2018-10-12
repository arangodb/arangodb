/*
    Copyright 2013 Christian Henning
    Use, modification and distribution are subject to the Boost Software License,
    Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
*/

#include <boost/test/unit_test.hpp>

#include <boost/type_traits/is_same.hpp>

#include <boost/gil.hpp>
#include <boost/gil/extension/toolbox/metafunctions/is_bit_aligned.hpp>

using namespace boost;
using namespace gil;

BOOST_AUTO_TEST_SUITE( toolbox_tests )

BOOST_AUTO_TEST_CASE( is_bit_aligned_test )
{
    typedef bit_aligned_image1_type< 4, gray_layout_t>::type image_t;
    BOOST_STATIC_ASSERT(( is_bit_aligned< image_t::view_t::value_type >::value ));
}

BOOST_AUTO_TEST_SUITE_END()
