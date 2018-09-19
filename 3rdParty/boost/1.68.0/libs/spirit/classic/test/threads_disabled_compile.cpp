/*=============================================================================
    Copyright (c) 2017 Nikita Kniazev
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

// Spirit should not include any Boost.Thread header until user explicitly
// requested threadsafe support with BOOST_SPIRIT_THREADSAFE defined.

#undef BOOST_SPIRIT_THREADSAFE
#ifndef BOOST_DISABLE_THREADS
# define BOOST_DISABLE_THREADS
#endif
#include <boost/spirit/include/classic.hpp>

int main() { return 0; }
