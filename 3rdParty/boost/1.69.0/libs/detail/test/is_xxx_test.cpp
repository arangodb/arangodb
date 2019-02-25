
// Copyright 2018 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/detail/is_xxx.hpp>
#include <boost/static_assert.hpp>

namespace is_xxx_test {
    template <typename T> struct thing1 {};
    template <typename T1, typename T2 = int> struct thing2 {};
}

BOOST_DETAIL_IS_XXX_DEF(thing1, is_xxx_test::thing1, 1);
BOOST_DETAIL_IS_XXX_DEF(thing2, is_xxx_test::thing2, 2);

BOOST_STATIC_ASSERT((is_thing1<is_xxx_test::thing1<int> >::value));
BOOST_STATIC_ASSERT((!is_thing1<is_xxx_test::thing2<int> >::value));
BOOST_STATIC_ASSERT((!is_thing2<is_xxx_test::thing1<int> >::value));
BOOST_STATIC_ASSERT((is_thing2<is_xxx_test::thing2<int> >::value));
BOOST_STATIC_ASSERT((is_thing2<is_xxx_test::thing2<int, float> >::value));

int main() {}
