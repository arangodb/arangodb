// Copyright 2019 Henry Schreiner, Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

/*
  For a header-only library, it is important to not accidentally violate the
  One-Definition-Rule (ODR), which causes linker errors. The ODR is violated
  when a non-templated function declared in a header is not inlined, and that
  header is then included in several translation units which are then linked
  together.

  We carry out this test by including all headers in two separate translation
  units which are then linked together. There is an additional test called
  check_odr_test.py which checks that "odr_test.cpp" includes all headers.
*/

#include "odr_test.cpp"

int main() { return 0; }
