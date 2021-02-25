#include <frozen/string.h>
#include <frozen/unordered_set.h>
#include <iostream>
#include <unordered_set>

#include "bench.hpp"
#include "catch.hpp"

TEST_CASE("singleton frozen unordered set", "[unordered set]") {
  constexpr frozen::unordered_set<int, 1> ze_set{1};

  constexpr auto empty = ze_set.empty();
  REQUIRE(!empty);

  constexpr auto size = ze_set.size();
  REQUIRE(size == 1);

  constexpr auto max_size = ze_set.max_size();
  REQUIRE(max_size == 1);

  constexpr auto nocount = ze_set.count(3);
  REQUIRE(nocount == 0);

  constexpr auto count = ze_set.count(1);
  REQUIRE(count == 1);

  auto notfound = ze_set.find(3);
  REQUIRE(notfound == ze_set.end());

  auto found = ze_set.find(1);
  REQUIRE(found == ze_set.begin());

  auto range = ze_set.equal_range(1);
  REQUIRE(std::get<0>(range) == ze_set.begin());
  REQUIRE(std::get<1>(range) == ze_set.end());

  auto begin = ze_set.begin(), end = ze_set.end();
  REQUIRE(begin != end);

  // auto constexpr key_hash = ze_set.hash_function();
  // auto constexpr key_hashed = key_hash(1);
  // REQUIRE(key_hashed);

  auto constexpr key_eq = ze_set.key_eq();
  auto constexpr value_comparison = key_eq(11, 11);
  REQUIRE(value_comparison);

  auto cbegin = ze_set.cbegin(), cend = ze_set.cend();
  REQUIRE(cbegin < cend);

  std::for_each(ze_set.begin(), ze_set.end(), [](int) {});
}

TEST_CASE("tripleton str frozen unordered set", "[unordered set]") {
  constexpr frozen::unordered_set<uint16_t, 3> ze_set{1, 2, 3};

  constexpr auto empty = ze_set.empty();
  REQUIRE(!empty);

  constexpr auto size = ze_set.size();
  REQUIRE(size == 3);

  constexpr auto max_size = ze_set.max_size();
  REQUIRE(max_size == 3);

  constexpr auto nocount = ze_set.count(4);
  REQUIRE(nocount == 0);

  constexpr auto count = ze_set.count(1);
  REQUIRE(count == 1);

  auto notfound = ze_set.find(4);
  REQUIRE(notfound == ze_set.end());

  auto found = ze_set.find(1);
  REQUIRE(found == ze_set.begin());

  auto range = ze_set.equal_range(1);
  REQUIRE(std::get<0>(range) != ze_set.end());

  auto begin = ze_set.begin(), end = ze_set.end();
  REQUIRE(begin != end);

  // auto constexpr key_hash = ze_set.hash_function();
  // auto constexpr key_hashed = key_hash(1);
  // REQUIRE(key_hashed);

  auto constexpr key_eq = ze_set.key_eq();
  auto constexpr value_comparison = key_eq(11, 11);
  REQUIRE(value_comparison);

  auto cbegin = ze_set.cbegin(), cend = ze_set.cend();
  REQUIRE(cbegin != cend);

  std::for_each(ze_set.begin(), ze_set.end(), [](uint16_t const &) {});
}

TEST_CASE("frozen::unordered_set<int> <> std::unordered_set",
          "[unordered_set]") {

#define INIT_SEQ                                                               \
  19, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 111, 1112, 1115, 1118, 1110, 1977, 177,  \
      277, 477, 577, 677, 777, 877, 977, 1077, 1177, 11177, 111277, 111577,    \
      111877, 111077, 1999, 199, 299, 499, 599, 699, 799, 899, 999, 1099,      \
      1199, 11199, 111299, 111599, 111899, 111099, 197799, 17799, 27799,       \
      47799, 57799, 67799, 77799, 87799, 97799, 107799, 117799, 1117799,       \
      11127799, 11157799, 11187799, 11107799, 1988, 188, 288, 488, 588, 688,   \
      788, 888, 988, 1088, 1188, 11188, 111288, 111588, 111888, 111088,        \
      197788, 17788, 27788, 47788, 57788, 67788, 77788, 87788, 97788, 107788,  \
      117788, 1117788, 11127788, 11157788, 11187788, 11107788, 199988, 19988,  \
      29988, 49988, 59988, 69988, 79988, 89988, 99988, 109988, 119988,         \
      1119988, 11129988, 11159988, 11189988, 11109988, 19779988, 1779988,      \
      2779988, 4779988, 5779988, 6779988, 7779988, 8779988, 9779988, 10779988, \
      11779988, 111779988, 1112779988, 1115779988, 1118779988, 1110779988, 456

  const std::unordered_set<int> std_set = {INIT_SEQ};
  constexpr frozen::unordered_set<int, 129> frozen_set = {INIT_SEQ};
  {
    REQUIRE(std_set.size() == frozen_set.size());
    for (auto v : std_set)
      REQUIRE(frozen_set.count(v));
    for (auto v : frozen_set)
      REQUIRE(std_set.count(v));
  }

}

TEST_CASE("frozen::unordered_set with enum keys", "[unordered_set]") {
  enum class some_enum {
    A,B,C
  };
  constexpr frozen::unordered_set<some_enum, 2> frozen_set = { some_enum::A, some_enum::B };
  REQUIRE(frozen_set.count(some_enum::A) == 1);
  REQUIRE(frozen_set.count(some_enum::C) == 0);
}

TEST_CASE("frozen::unordered_set <> frozen::make_unordered_set", "[unordered_set]") {
  constexpr frozen::unordered_set<int, 129> frozen_set = { INIT_SEQ };
  constexpr auto frozen_set2 = frozen::make_unordered_set<int>({INIT_SEQ});

  SECTION("checking size and content") {
    REQUIRE(frozen_set.size() == frozen_set2.size());
    for (auto v : frozen_set2)
      REQUIRE(frozen_set.count(v));
    for (auto v : frozen_set)
      REQUIRE(frozen_set2.count(v));
  }
}
TEST_CASE("frozen::unordered_set constexpr", "[unordered_set]") {
  constexpr frozen::unordered_set<unsigned, 2> ce = {3, 11};
  static_assert(ce.begin() +2 == ce.end(), "");
  static_assert(ce.size() == 2, "");
  static_assert(ce.count(3), "");
  static_assert(!ce.count(0), "");
  static_assert(ce.find(0) == ce.end(), "");
}
