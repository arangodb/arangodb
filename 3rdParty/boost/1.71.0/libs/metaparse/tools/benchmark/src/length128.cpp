// Copyright Abel Sinkovics (abel@sinkovics.hu) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// n in [0..2048), step 2
// x_axis_label: Length of the 128 strings
// desc: 128 strings with increasing length.
// modes: BOOST_METAPARSE_STRING, manual

\#define BOOST_METAPARSE_LIMIT_STRING_SIZE $n

\#include <benchmark_util.hpp>

#for j in range(0, 128)
TEST_STRING($random_string($n))
#end for

