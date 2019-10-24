// Copyright Antony Polukhin, 2016-2019.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_STACKTRACE_INTERNAL_BUILD_LIBS
#define BOOST_STACKTRACE_LINK

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#include <boost/stacktrace/detail/frame_unwind.ipp>
#include <boost/stacktrace/safe_dump_to.hpp>
