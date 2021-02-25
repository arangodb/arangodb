#include <algorithm>
#include <frozen/bits/algorithms.h>
#include <iostream>

#include "bench.hpp"
#include "catch.hpp"

TEST_CASE("next_highest_power_of_two", "[algorithm]") {
  REQUIRE(frozen::bits::next_highest_power_of_two(1) == 1);
  REQUIRE(frozen::bits::next_highest_power_of_two(2) == 2);
  REQUIRE(frozen::bits::next_highest_power_of_two(3) == 4);
  REQUIRE(frozen::bits::next_highest_power_of_two(4) == 4);
  REQUIRE(frozen::bits::next_highest_power_of_two(5) == 8);
  REQUIRE(frozen::bits::next_highest_power_of_two(6) == 8);
  REQUIRE(frozen::bits::next_highest_power_of_two(7) == 8);
  REQUIRE(frozen::bits::next_highest_power_of_two(8) == 8);
  REQUIRE(frozen::bits::next_highest_power_of_two(16) == 16);
}

TEST_CASE("log", "[algorithm]") {
  REQUIRE(frozen::bits::log(1) == 0);
  REQUIRE(frozen::bits::log(2) == 1);
  REQUIRE(frozen::bits::log(3) == 1);
  REQUIRE(frozen::bits::log(4) == 2);
  REQUIRE(frozen::bits::log(5) == 2);
  REQUIRE(frozen::bits::log(7) == 2);
  REQUIRE(frozen::bits::log(8) == 3);
  REQUIRE(frozen::bits::log(16) == 4);
  REQUIRE(frozen::bits::log(32) == 5);
}
