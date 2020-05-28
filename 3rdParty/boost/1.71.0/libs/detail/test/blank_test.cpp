
// Copyright 2018 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/blank.hpp>
#include <boost/static_assert.hpp>
#include <cassert>

#if !defined(BOOST_NO_IOSTREAM)
#include <sstream>
#endif

int main() {
    BOOST_STATIC_ASSERT((boost::is_pod<boost::blank>::value));
    BOOST_STATIC_ASSERT((boost::is_empty<boost::blank>::value));
    BOOST_STATIC_ASSERT((boost::is_stateless<boost::blank>::value));

    boost::blank b1,b2;
    assert(b1 == b2);
    assert(b1 <= b2);
    assert(b1 >= b2);
    assert(!(b1 != b2));
    assert(!(b1 < b2));
    assert(!(b1 > b2));

#if !defined(BOOST_NO_IOSTREAM)
    std::stringstream s;
    s << "(" << b1 << ")";
    assert(s.str() == "()");
#endif
}
