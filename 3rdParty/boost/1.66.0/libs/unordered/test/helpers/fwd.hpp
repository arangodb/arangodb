
// Copyright 2006-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(BOOST_UNORDERED_TEST_HELPERS_FWD_HEADER)
#define BOOST_UNORDERED_TEST_HELPERS_FWD_HEADER

#include <string>

namespace test {
  typedef enum {
    default_generator,
    generate_collisions,
    limited_range
  } random_generator;

  int generate(int const*, random_generator);
  char generate(char const*, random_generator);
  signed char generate(signed char const*, random_generator);
  std::string generate(std::string const*, random_generator);
  float generate(float const*, random_generator);

  struct base_type
  {
  } base;
  struct derived_type : base_type
  {
  } derived;
}

#endif
