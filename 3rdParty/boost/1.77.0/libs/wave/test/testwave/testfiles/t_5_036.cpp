/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2020 Jeff Trull. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    Part of the test included in this file was taken from a tweet by
    Eric Niebler: https://twitter.com/ericniebler/status/1252660334545866752
=============================================================================*/

// Test __LINE__ and __FILE__ in a context where the invocation of the macro
// that uses them is split across two lines

// __LINE__ should reflect the position where the macros are expanded
#define FOO(X) __LINE__ __FILE__ BAR
#define BAR(X) __LINE__ __FILE__
FOO(X)
  (Y)
//R #line 19 "t_5_036.cpp"
//R 19 "$F" 19 "$F"

// now use those same macros in a different file -
// __FILE__ should report that.
#include "t_5_036.hpp"
//R #line 11 "t_5_036.hpp"
//R 11 "$P(t_5_036.hpp)" 11 "$P(t_5_036.hpp)"
