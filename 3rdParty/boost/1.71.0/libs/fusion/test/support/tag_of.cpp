/*=============================================================================
    Copyright (c) 2018 Louis Dionne

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/fusion/support/tag_of.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>


// Make sure tag_of can be used with an incomplete type.
struct incomplete;
typedef boost::fusion::traits::tag_of<incomplete>::type Tag;
BOOST_STATIC_ASSERT((boost::is_same<Tag, boost::fusion::non_fusion_tag>::value));

int main() { }
