
// Copyright 2017 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off
#include "../helpers/prefix.hpp"
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include "../helpers/postfix.hpp"
// clang-format on

#include "../helpers/test.hpp"
#include <map>

// Pretty inefficient, but the test is fast enough.
// Might be too slow if we had larger primes?
bool is_prime(std::size_t x)
{
  if (x == 2) {
    return true;
  } else if (x == 1 || x % 2 == 0) {
    return false;
  } else {
    // y*y <= x had rounding errors, so instead use y <= (x/y).
    for (std::size_t y = 3; y <= (x / y); y += 2) {
      if (x % y == 0) {
        return false;
        break;
      }
    }

    return true;
  }
}

void test_next_prime(std::size_t value)
{
  std::size_t x = boost::unordered::detail::next_prime(value);
  BOOST_TEST(is_prime(x));
  BOOST_TEST(x >= value);
}

void test_prev_prime(std::size_t value)
{
  std::size_t x = boost::unordered::detail::prev_prime(value);
  BOOST_TEST(is_prime(x));
  BOOST_TEST(x <= value);
  if (x > value) {
    BOOST_LIGHTWEIGHT_TEST_OSTREAM << x << "," << value << std::endl;
  }
}

UNORDERED_AUTO_TEST (next_prime_test) {
  BOOST_TEST(!is_prime(0));
  BOOST_TEST(!is_prime(1));
  BOOST_TEST(is_prime(2));
  BOOST_TEST(is_prime(3));
  BOOST_TEST(is_prime(13));
  BOOST_TEST(!is_prime(4));
  BOOST_TEST(!is_prime(100));

  BOOST_TEST(boost::unordered::detail::next_prime(0) > 0);

  // test_prev_prime doesn't work for values less than 17.
  // Which should be okay, unless an allocator has a really tiny
  // max_size?
  const std::size_t min_prime = 17;

  // test_next_prime doesn't work for values greater than this,
  // which might be a problem if you've got terrabytes of memory?
  // I seriously doubt the container would work well at such sizes
  // regardless.
  const std::size_t max_prime = 4294967291ul;

  std::size_t i;

  BOOST_TEST(is_prime(min_prime));
  BOOST_TEST(is_prime(max_prime));

  for (i = 0; i < 10000; ++i) {
    if (i < min_prime) {
      BOOST_TEST(boost::unordered::detail::prev_prime(i) == min_prime);
    } else {
      test_prev_prime(i);
    }
    test_next_prime(i);
  }

  std::size_t last = i - 1;
  for (; i > last; last = i, i += i / 5) {
    if (i > max_prime) {
      BOOST_TEST(boost::unordered::detail::next_prime(i) == max_prime);
    } else {
      test_next_prime(i);
    }
    test_prev_prime(i);
  }
}

RUN_TESTS()
