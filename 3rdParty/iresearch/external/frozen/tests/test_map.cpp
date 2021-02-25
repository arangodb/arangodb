#include <algorithm>
#include <frozen/map.h>
#include <iostream>
#include <map>

#include "bench.hpp"
#include "catch.hpp"

TEST_CASE("empty frozen map", "[map]") {
  constexpr frozen::map<int, float, 0> ze_map{};

  constexpr auto empty = ze_map.empty();
  REQUIRE(empty);

  constexpr auto size = ze_map.size();
  REQUIRE(size == 0);

  constexpr auto max_size = ze_map.max_size();
  REQUIRE(max_size == 0);

  constexpr auto count = ze_map.count(3);
  REQUIRE(count == 0);

  constexpr auto find = ze_map.find(5);
  REQUIRE(find == ze_map.end());

  constexpr auto range = ze_map.equal_range(0);
  REQUIRE(std::get<0>(range) == ze_map.end());
  REQUIRE(std::get<1>(range) == ze_map.end());

  constexpr auto lower_bound = ze_map.lower_bound(1);
  REQUIRE(lower_bound == ze_map.end());

  constexpr auto upper_bound = ze_map.upper_bound(1);
  REQUIRE(upper_bound == ze_map.end());

  auto constexpr begin = ze_map.begin(), end = ze_map.end();
  REQUIRE(begin == end);

  auto constexpr key_comp = ze_map.key_comp();
  auto constexpr key_comparison = key_comp(1, 2);
  REQUIRE(key_comparison);

  auto constexpr value_comp = ze_map.value_comp();
  auto constexpr value_comparison = value_comp(11, 12);
  REQUIRE(value_comparison);

  auto constexpr cbegin = ze_map.cbegin(), cend = ze_map.cend();
  REQUIRE(cbegin == cend);

  std::for_each(ze_map.begin(), ze_map.end(), [](std::tuple<int, float>) {});
  REQUIRE(std::distance(ze_map.rbegin(), ze_map.rend()) == 0);
  REQUIRE(std::count(ze_map.crbegin(), ze_map.crend(),
                     decltype(ze_map)::value_type(3, 5.3f)) == 0);
}

TEST_CASE("singleton frozen map", "[map]") {
  constexpr frozen::map<short, double, 1> ze_map = {{1, 3.14}};

  constexpr auto empty = ze_map.empty();
  REQUIRE(!empty);

  constexpr auto size = ze_map.size();
  REQUIRE(size == 1);

  constexpr auto max_size = ze_map.max_size();
  REQUIRE(max_size == 1);

  constexpr auto count1 = ze_map.count(1);
  REQUIRE(count1 == 1);

  constexpr auto count2 = ze_map.count(2);
  REQUIRE(count2 == 0);

  const auto find1 = ze_map.find(1);
  REQUIRE(find1 == ze_map.begin());
  REQUIRE(std::get<0>(*find1) == 1);
  REQUIRE(std::get<1>(*find1) == 3.14);

  const auto find5 = ze_map.find(5);
  REQUIRE(find5 == ze_map.end());

  const auto range0 = ze_map.equal_range(0);
  REQUIRE(std::get<0>(range0) == ze_map.end());
  REQUIRE(std::get<1>(range0) == ze_map.end());

  const auto range1 = ze_map.equal_range(1);
  REQUIRE(std::get<0>(range1) == ze_map.begin());
  REQUIRE(std::get<1>(range1) == ze_map.end());

  const auto lower_bound0 = ze_map.lower_bound(0);
  REQUIRE(lower_bound0 == ze_map.end());

  const auto lower_bound1 = ze_map.lower_bound(1);
  REQUIRE(lower_bound1 == ze_map.find(1));

  const auto lower_bound2 = ze_map.lower_bound(2);
  REQUIRE(lower_bound2 == ze_map.end());

  const auto upper_bound0 = ze_map.upper_bound(0);
  REQUIRE(upper_bound0 == ze_map.end());

  const auto upper_bound1 = ze_map.upper_bound(1);
  REQUIRE(upper_bound1 == ze_map.end());

  const auto upper_bound2 = ze_map.upper_bound(2);
  REQUIRE(upper_bound2 == ze_map.end());

  auto const begin = ze_map.begin(), end = ze_map.end();
  REQUIRE((begin + 1) == end);

  auto const key_comp = ze_map.key_comp();
  auto const key_comparison = key_comp(1, 2);
  REQUIRE(key_comparison);

  auto const value_comp = ze_map.value_comp();
  auto const value_comparison = value_comp(11, 12);
  REQUIRE(value_comparison);

  auto const cbegin = ze_map.cbegin(), cend = ze_map.cend();
  REQUIRE(cbegin == (cend - 1));

  std::for_each(ze_map.begin(), ze_map.end(), [](std::tuple<short, double>) {});
  REQUIRE(std::distance(ze_map.rbegin(), ze_map.rend()) == 1);
  REQUIRE(std::count(ze_map.crbegin(), ze_map.crend(),
                     decltype(ze_map)::value_type(3, 14)) == 0);
  REQUIRE(std::count(ze_map.crbegin(), ze_map.crend(),
                     decltype(ze_map)::value_type(1, 3.14)) == 1);
}

TEST_CASE("triple frozen map", "[map]") {
  constexpr frozen::map<unsigned long, bool, 3> ze_map{
      {10, true}, {20, false}, {30, true}};

  constexpr auto empty = ze_map.empty();
  REQUIRE(!empty);

  constexpr auto size = ze_map.size();
  REQUIRE(size == 3);

  constexpr auto max_size = ze_map.max_size();
  REQUIRE(max_size == 3);

  constexpr auto count1 = ze_map.count(1);
  REQUIRE(count1 == 0);

  constexpr auto count10 = ze_map.count(10);
  REQUIRE(count10 == 1);

  const auto find10 = ze_map.find(10);
  REQUIRE(find10 == ze_map.begin());
  REQUIRE(std::get<0>(*find10) == 10);
  REQUIRE(std::get<1>(*find10) == true);

  const auto find15 = ze_map.find(15);
  REQUIRE(find15 == ze_map.end());

  const auto range0 = ze_map.equal_range(0);
  REQUIRE(std::get<0>(range0) == ze_map.end());
  REQUIRE(std::get<1>(range0) == ze_map.end());

  const auto range1 = ze_map.equal_range(10);
  REQUIRE(std::get<0>(range1) == ze_map.begin());
  REQUIRE(std::get<1>(range1) == ze_map.begin() + 1);

  const auto lower_bound0 = ze_map.lower_bound(0);
  REQUIRE(lower_bound0 == ze_map.end());

  for (auto val : ze_map) {
    const auto lower_bound = ze_map.lower_bound(std::get<0>(val));
    REQUIRE(lower_bound == ze_map.find(std::get<0>(val)));
  }

  const auto lower_bound2 = ze_map.lower_bound(40);
  REQUIRE(lower_bound2 == ze_map.end());

  const auto upper_bound0 = ze_map.upper_bound(0);
  REQUIRE(upper_bound0 == ze_map.end());

  const auto upper_bound1 = ze_map.upper_bound(10);
  REQUIRE(upper_bound1 == (ze_map.begin() + 1));

  const auto upper_bound2 = ze_map.upper_bound(40);
  REQUIRE(upper_bound2 == ze_map.end());

  auto const begin = ze_map.begin(), end = ze_map.end();
  REQUIRE((begin + ze_map.size()) == end);

  auto const key_comp = ze_map.key_comp();
  auto const key_comparison = key_comp(1, 2);
  REQUIRE(key_comparison);

  auto const value_comp = ze_map.value_comp();
  auto const value_comparison = value_comp(11, 12);
  REQUIRE(value_comparison);

  auto const cbegin = ze_map.cbegin(), cend = ze_map.cend();
  REQUIRE(cbegin == (cend - ze_map.size()));

  std::for_each(ze_map.begin(), ze_map.end(), [](std::tuple<unsigned long, bool>) {});
  REQUIRE(std::distance(ze_map.rbegin(), ze_map.rend()) == ze_map.size());
  REQUIRE(std::count(ze_map.crbegin(), ze_map.crend(),
                     decltype(ze_map)::value_type(3, 14)) == 0);
  REQUIRE(std::count(ze_map.crbegin(), ze_map.crend(),
                     decltype(ze_map)::value_type(20, false)) == 1);
}

TEST_CASE("frozen::map <> std::map", "[map]") {
#define INIT_SEQ                                                               \
    {19, 12}, {1, 12}, {2, 12}, {4, 12}, {5, 12}, {6, 12}, {7, 12}, {8, 12},   \
        {9, 12}, {10, 12}, {11, 12}, {111, 12}, {1112, 12}, {1115, 12},        \
        {1118, 12}, {1110, 12}, {1977, 12}, {177, 12}, {277, 12}, {477, 12},   \
        {577, 12}, {677, 12}, {777, 12}, {877, 12}, {977, 12}, {1077, 12},     \
        {1177, 12}, {11177, 12}, {111277, 12}, {111577, 12}, {111877, 12},     \
        {111077, 12}, {1999, 12}, {199, 12}, {299, 12}, {499, 12}, {599, 12},  \
        {699, 12}, {799, 12}, {899, 12}, {999, 12}, {1099, 12}, {1199, 12},    \
        {11199, 12}, {111299, 12}, {111599, 12}, {111899, 12}, {111099, 12},   \
        {197799, 12}, {17799, 12}, {27799, 12}, {47799, 12}, {57799, 12},      \
        {67799, 12}, {77799, 12}, {87799, 12}, {97799, 12}, {107799, 12},      \
        {117799, 12}, {1117799, 12}, {11127799, 12}, {11157799, 12},           \
        {11187799, 12}, {11107799, 12}, {1988, 12}, {188, 12}, {288, 12},      \
        {488, 12}, {588, 12}, {688, 12}, {788, 12}, {888, 12}, {988, 12},      \
        {1088, 12}, {1188, 12}, {11188, 12}, {111288, 12}, {111588, 12},       \
        {111888, 12}, {111088, 12}, {197788, 12}, {17788, 12}, {27788, 12},    \
        {47788, 12}, {57788, 12}, {67788, 12}, {77788, 12}, {87788, 12},       \
        {97788, 12}, {107788, 12}, {117788, 12}, {1117788, 12},                \
        {11127788, 12}, {11157788, 12}, {11187788, 12}, {11107788, 12},        \
        {199988, 12}, {19988, 12}, {29988, 12}, {49988, 12}, {59988, 12},      \
        {69988, 12}, {79988, 12}, {89988, 12}, {99988, 12}, {109988, 12},      \
        {119988, 12}, {1119988, 12}, {11129988, 12}, {11159988, 12},           \
        {11189988, 12}, {11109988, 12}, {19779988, 12}, {1779988, 12},         \
        {2779988, 12}, {4779988, 12}, {5779988, 12}, {6779988, 12},            \
        {7779988, 12}, {8779988, 12}, {9779988, 12}, {10779988, 12},           \
        {11779988, 12}, {111779988, 12}, {1112779988, 12}, {1115779988, 12},   \
        {1118779988, 12}, {                                                    \
      1110779988, 13                                                           \
    }

  const std::map<int, int> std_map = { INIT_SEQ };
  constexpr frozen::map<int, int, 128> frozen_map = { INIT_SEQ };
  SECTION("checking size and content") {
    REQUIRE(std_map.size() == frozen_map.size());
    for (auto v : std_map) {
      REQUIRE(frozen_map.count(std::get<0>(v)));
      REQUIRE(frozen_map.find(std::get<0>(v))->second == v.second);
    }
    for (auto v : frozen_map)
      REQUIRE(std_map.count(std::get<0>(v)));
  }

  static_assert(std::is_same<typename decltype(std_map)::key_type,
                             typename decltype(frozen_map)::key_type>::value, "");
  static_assert(std::is_same<typename decltype(std_map)::mapped_type,
                             typename decltype(frozen_map)::mapped_type>::value, "");
}

TEST_CASE("frozen::map <> frozen::make_map", "[map]") {
  constexpr frozen::map<int, int, 128> frozen_map = { INIT_SEQ };
  constexpr auto frozen_map2 = frozen::make_map<int, int>({INIT_SEQ});

  SECTION("checking size and content") {
    REQUIRE(frozen_map.size() == frozen_map2.size());
    for (auto v : frozen_map2)
      REQUIRE(frozen_map.count(std::get<0>(v)));
    for (auto v : frozen_map)
      REQUIRE(frozen_map2.count(std::get<0>(v)));
  }

  constexpr frozen::map<int, short, 0> frozen_empty_map = {};
  constexpr auto frozen_empty_map2 = frozen::make_map<int, short>();
  constexpr auto frozen_empty_map3 = frozen::make_map<int, short>({});

  SECTION("checking empty map") {
    REQUIRE(frozen_empty_map.empty());
    REQUIRE(frozen_empty_map.size() == 0);
    REQUIRE(frozen_empty_map.begin() == frozen_empty_map.end());

    REQUIRE(frozen_empty_map2.empty());
    REQUIRE(frozen_empty_map2.size() == 0);
    REQUIRE(frozen_empty_map2.begin() == frozen_empty_map2.end());

    REQUIRE(frozen_empty_map3.empty());
    REQUIRE(frozen_empty_map3.size() == 0);
    REQUIRE(frozen_empty_map3.begin() == frozen_empty_map3.end());
  }
}


TEST_CASE("frozen::map constexpr", "[map]") {
  constexpr frozen::map<unsigned, unsigned, 2> ce = {{3,4}, {11,12}};
  static_assert(*ce.begin() == std::pair<unsigned, unsigned>{3,4}, "");
  static_assert(*(ce.begin() + 1) == std::pair<unsigned, unsigned>{11,12}, "");
  static_assert(ce.size() == 2, "");
  static_assert(ce.count(3), "");
  static_assert(!ce.count(0), "");
  static_assert(ce.find(0) == ce.end(), "");
}
