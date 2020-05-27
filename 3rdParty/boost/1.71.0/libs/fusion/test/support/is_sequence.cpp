/*=============================================================================
    Copyright (c) 2018 Louis Dionne

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/support/is_sequence.hpp>
#include <boost/static_assert.hpp>


// Make sure fusion::is_sequence and fusion::is_native_fusion_sequence can be
// used with an incomplete type.
struct incomplete;
BOOST_STATIC_ASSERT(!boost::fusion::traits::is_sequence<incomplete>::value);
BOOST_STATIC_ASSERT(!boost::fusion::traits::is_native_fusion_sequence<incomplete>::value);

int main() { }
