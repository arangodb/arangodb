// Copyright (c) 2014 Kohei Takahashi.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://www.boost.org for most recent version including documentation.

#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <boost/config/pragma_message.hpp>

#if BOOST_WORKAROUND(BOOST_MSVC, < 1600)

BOOST_PRAGMA_MESSAGE("Skipping test on msvc-9.0 and below")
int main() {}

#elif defined(BOOST_GCC) && __cplusplus < 201100

BOOST_PRAGMA_MESSAGE("Skipping test on g++ in C++03 mode")
int main() {}

#else

#include <utility>
#include <boost/fusion/adapted/std_pair.hpp>

#define TUPLE      std::pair
#define MAKE_TUPLE std::make_pair

#include "detail/zip_iterator_test.ipp"

#endif
