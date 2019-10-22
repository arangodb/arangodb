/*=============================================================================
    Copyright (c) 2018 Nikita Kniazev

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/support/is_view.hpp>
#include <boost/static_assert.hpp>


// Make sure fusion::is_view can be used with non fusion types.
struct incomplete;
BOOST_STATIC_ASSERT(!boost::fusion::traits::is_view<incomplete>::value);

int main() { }
