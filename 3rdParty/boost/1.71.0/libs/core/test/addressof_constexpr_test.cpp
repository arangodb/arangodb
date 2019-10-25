/*
Copyright 2017 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/

#include <boost/core/addressof.hpp>
#include <boost/static_assert.hpp>

#if !defined(BOOST_CORE_NO_CONSTEXPR_ADDRESSOF)
struct Type { };

static int v1 = 0;
static Type v2 = { };

BOOST_STATIC_ASSERT(boost::addressof(v1) == &v1);
BOOST_STATIC_ASSERT(boost::addressof(v2) == &v2);
#endif
