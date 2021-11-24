// Copyright Abel Sinkovics (abel@sinkovics.hu) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// n in [1..2048), step 2
// x_axis_label: Maximum length of strings
// desc: 100 one character long strings with increasing maximum length.
// modes: BOOST_METAPARSE_STRING

\#define BOOST_METAPARSE_LIMIT_STRING_SIZE $n

\#include <benchmark_util.hpp>

#for j in range(0, 10)
TEST_STRING(BOOST_METAPARSE_STRING("\x0$j"))
#end for
#for j in range(10, 100)
TEST_STRING(BOOST_METAPARSE_STRING("\x$j"))
#end for

