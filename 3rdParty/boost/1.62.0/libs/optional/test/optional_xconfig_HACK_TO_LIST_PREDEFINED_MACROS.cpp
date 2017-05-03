// Copyright (C) 2015 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#include "boost/core/lightweight_test.hpp"
#include "boost/optional/detail/optional_config.hpp"
#include <string>

int main()
{
#if defined(__GNUC__)

  std::string emptys;
  
#ifdef BOOST_INTEL_CXX_VERSION
  BOOST_TEST_EQ(emptys, "HAS INTEL INSIDE");
#else
  BOOST_TEST_EQ(emptys, "NO INTEL INSIDE");
#endif
  
#if !defined BOOST_NO_CXX11_RVALUE_REFERENCES
  BOOST_TEST_EQ(emptys, "HAS RVALUE REFS");
#else
  BOOST_TEST_EQ(emptys, "NO RVALUE REFS");
#endif

  int empty = -1;
  BOOST_TEST_EQ(empty, __GNUC__);
  BOOST_TEST_EQ(empty, __GNUC_MINOR__);
  BOOST_TEST_EQ(empty, __GNUC_PATCHLEVEL__);
  BOOST_TEST_EQ(empty, __cplusplus);
#endif
  return boost::report_errors();
}
