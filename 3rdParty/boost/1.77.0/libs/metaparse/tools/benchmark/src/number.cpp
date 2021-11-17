// Copyright Abel Sinkovics (abel@sinkovics.hu) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// n in [0..1024), step 2
// x_axis_label: Number of strings
// desc: Increasing number of strings with 64 length.
// modes: BOOST_METAPARSE_STRING, manual

\#define BOOST_METAPARSE_LIMIT_STRING_SIZE 32

\#include <benchmark_util.hpp>

#for j in range(0, $n)
TEST_STRING($random_string(32))
#end for

