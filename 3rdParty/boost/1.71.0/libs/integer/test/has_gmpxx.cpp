//  Copyright John Maddock 2008.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <gmpxx.h>

// On Linux, libgmpxx is built with gcc and libstdc++. When the user application, such as tests, are built against libc++,
// linking fails because of the C++ standard library symbol names mismatch. So fail the test if we're not using libstdc++.
#if defined(__linux__) || defined(__linux) || defined(linux)
#include <utility>
#if !defined(__GLIBCPP__) && !defined(__GLIBCXX__)
#error "libgmpxx is not supported on this platform with this C++ standard library"
#endif
#endif
