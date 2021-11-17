
// Copyright 2005-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// This uses std::rand to generate random values for tests.
// Which is not good as different platforms will be running different tests.
// It would be much better to use Boost.Random, but it doesn't
// support all the compilers that I want to test on.

#if !defined(BOOST_UNORDERED_TEST_HELPERS_GENERATORS_HEADER)
#define BOOST_UNORDERED_TEST_HELPERS_GENERATORS_HEADER

#include "./fwd.hpp"
#include <boost/type_traits/add_const.hpp>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace test {
  struct seed_t
  {
    seed_t(unsigned int x)
    {
      using namespace std;
      srand(x);
    }
  };

  std::size_t random_value(std::size_t max)
  {
    using namespace std;
    return static_cast<std::size_t>(rand()) % max;
  }

  inline int generate(int const*, random_generator g)
  {
    using namespace std;
    int value = rand();
    if (g == limited_range) {
      value = value % 100;
    }
    return value;
  }

  inline char generate(char const*, random_generator)
  {
    using namespace std;
    return static_cast<char>((rand() >> 1) % (128 - 32) + 32);
  }

  inline signed char generate(signed char const*, random_generator)
  {
    using namespace std;
    return static_cast<signed char>(rand());
  }

  inline std::string generate(std::string const*, random_generator g)
  {
    using namespace std;

    char* char_ptr = 0;

    std::string result;

    if (g == limited_range) {
      std::size_t length = test::random_value(2) + 2;

      char const* strings[] = {"'vZh(3~ms", "%m", "_Y%U", "N'Y", "4,J_J"};
      for (std::size_t i = 0; i < length; ++i) {
        result += strings[random_value(sizeof(strings) / sizeof(strings[0]))];
      }
    } else {
      std::size_t length = test::random_value(10) + 1;
      for (std::size_t i = 0; i < length; ++i) {
        result += generate(char_ptr, g);
      }
    }

    return result;
  }

  float generate(float const*, random_generator g)
  {
    using namespace std;
    int x = 0;
    int value = generate(&x, g);
    return (float)value / (float)RAND_MAX;
  }
}

#endif
