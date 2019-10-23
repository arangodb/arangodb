// Copyright Abel Sinkovics (abel@sinkovics.hu) 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_METAPARSE_LIMIT_STRING_SIZE 4
#include <boost/metaparse/string.hpp>

namespace
{
  typedef BOOST_METAPARSE_STRING("abcde") too_long;
}

