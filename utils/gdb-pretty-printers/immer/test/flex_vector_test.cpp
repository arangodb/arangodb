////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <csignal>
#include <fmt/core.h>
#include <immer/flex_vector.hpp>
#include <string>
#include <vector>

auto breakpoint() {
#ifndef WIN32
  raise(SIGTRAP);
#else
  // Note that I haven't actually tried whether this works with gdb on Windows.
  // But Windows has no SIGTRAP, so.
  raise(SIGINT);
#endif
}

auto get_testees() {
  using namespace std::string_literals;

  auto vec16 =
      immer::flex_vector<std::string, immer::default_memory_policy, 2, 2>(
          {" 0"s, " 1"s, " 2"s, " 3"s, " 4"s, " 5"s, " 6"s, " 7"s, " 8"s, " 9"s,
           "10"s, "11"s, "12"s, "13"s, "14"s, "15"s});
  auto vec32_bl2 =
      immer::flex_vector<std::string, immer::default_memory_policy, 2, 2>(
          {" 0"s, " 1"s, " 2"s, " 3"s, " 4"s, " 5"s, " 6"s, " 7"s,
           " 8"s, " 9"s, "10"s, "11"s, "12"s, "13"s, "14"s, "15"s,
           "16"s, "17"s, "18"s, "19"s, "20"s, "21"s, "22"s, "23"s,
           "24"s, "25"s, "26"s, "27"s, "28"s, "29"s, "30"s, "31"s});
  auto vec32_bl1 =
      immer::flex_vector<std::string, immer::default_memory_policy, 2, 1>(
          {" 0"s, " 1"s, " 2"s, " 3"s, " 4"s, " 5"s, " 6"s, " 7"s,
           " 8"s, " 9"s, "10"s, "11"s, "12"s, "13"s, "14"s, "15"s,
           "16"s, "17"s, "18"s, "19"s, "20"s, "21"s, "22"s, "23"s,
           "24"s, "25"s, "26"s, "27"s, "28"s, "29"s, "30"s, "31"s});
  auto vec32_bl0 =
      immer::flex_vector<std::string, immer::default_memory_policy, 2, 0>(
          {" 0"s, " 1"s, " 2"s, " 3"s, " 4"s, " 5"s, " 6"s, " 7"s,
           " 8"s, " 9"s, "10"s, "11"s, "12"s, "13"s, "14"s, "15"s,
           "16"s, "17"s, "18"s, "19"s, "20"s, "21"s, "22"s, "23"s,
           "24"s, "25"s, "26"s, "27"s, "28"s, "29"s, "30"s, "31"s});

  auto vec8_bl2 =
      immer::flex_vector<std::string, immer::default_memory_policy, 2, 2>(
          {" 0"s, " 1"s, " 2"s, " 3"s, " 4"s, " 5"s, " 6"s, " 7"s});

  auto vec8_bl0 =
      immer::flex_vector<std::string, immer::default_memory_policy, 2, 0>(
          {" 0"s, " 1"s, " 2"s, " 3"s, " 4"s, " 5"s, " 6"s, " 7"s});

  auto vec_relaxed_bl2 = vec8_bl2 + vec8_bl2;
  auto vec_relaxed_bl0 = vec8_bl0 + vec8_bl0;

  return std::tuple{
      immer::flex_vector<std::string>({}),
      immer::flex_vector<std::string>({"hello, world"s}),
      immer::flex_vector<std::string>({"hello"s, "world"s}),
      vec16,
      vec32_bl2,
      vec32_bl1,
      vec32_bl0,
      vec_relaxed_bl2,
      vec_relaxed_bl0,
  };
}

auto testee_to_pretty_string(auto testee) {
  // e.g. ```immer::flex_vector of length 2 = {"hello", "world"}```
  auto res = fmt::format("immer::flex_vector of length {}", testee.size());
  if (!testee.empty()) {
    res += " = {";
    res += '"' + testee[0] + '"';
    for (auto i = std::size_t{1}; i < testee.size(); ++i) {
      res += ", ";
      res += '"' + testee[i] + '"';
    }
    res += "}";
  }
  return res;
}

template<std::size_t... idxs>
auto gen_expected_impl(auto const& testees, std::index_sequence<idxs...>) {
  return std::array{(testee_to_pretty_string(std::get<idxs>(testees)))...};
}

auto gen_expected(auto testees) {
  auto constexpr n = std::tuple_size<decltype(testees)>::value;
  auto expected = std::array<std::string, n>();

  return gen_expected_impl(testees, std::make_index_sequence<n>());
}

auto run_test(auto const& testee, auto const& expected, auto i) {
  breakpoint();
}

template<std::size_t... idxs>
auto run_tests_impl(auto const& testees, auto const& expected,
                    std::index_sequence<idxs...>) {
  (run_test(std::get<idxs>(testees), expected[idxs], idxs), ...);
}

auto run_tests(auto testees, auto expected) {
  auto constexpr n = std::tuple_size<decltype(testees)>::value;
  return run_tests_impl(testees, expected, std::make_index_sequence<n>());
}

int main() {
  auto testees = get_testees();
  auto expected = gen_expected(testees);
  // in some settings, size() is inlined, so we make it accessible to gdb this
  // way.
  [[maybe_unused]] const auto n = expected.size();

  breakpoint();
  run_tests(testees, expected);

  return 0;
}
