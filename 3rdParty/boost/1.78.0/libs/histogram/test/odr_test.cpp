// Copyright 2019 Henry Schreiner, Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// The header windows.h and possibly others illegially do the following
#define small char
// which violates the C++ standard. We make sure here that including our headers work
// nevertheless by avoiding the preprocessing token `small`. For more details, see
// https://github.com/boostorg/histogram/issues/342

// include all Boost.Histogram header here; see odr_main_test.cpp for details
#include <boost/histogram.hpp>
#include <boost/histogram/ostream.hpp>
#include <boost/histogram/serialization.hpp>
