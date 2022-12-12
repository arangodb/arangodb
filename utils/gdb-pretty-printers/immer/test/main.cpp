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
#include <fmt/core.h>
#include <immer/flex_vector.hpp>
#include <string>
#include <vector>

//void clobber() { asm volatile("" : : : "memory"); }
//
//void escape(void* p) { asm volatile("" : : "g"(p) : "memory"); }

void stub() {}

int main() {
  using namespace std::string_literals;

  auto v = std::vector<std::string>({"hello"s, "world"s});

  auto vec1 = immer::flex_vector<std::string>({"hello"s, "world"s});

  auto vec2 = vec1.push_back("little").push_back("immer");

// Use a smaller B to get more interesting trees with fewer leafs
  auto vec3 = immer::flex_vector<std::string, immer::default_memory_policy, 2, 2>({
      " 0"s,
      " 1"s,
      " 2"s,
      " 3"s,
      " 4"s,
      " 5"s,
      " 6"s,
      " 7"s,
      " 8"s,
      " 9"s,
      "10"s,
      "11"s,
      "12"s,
      "13"s,
      "14"s,
      "15"s,
  });

  auto vec4 = immer::flex_vector<std::string, immer::default_memory_policy, 2, 2>({
      " 0"s,
      " 1"s,
      " 2"s,
      " 3"s,
      " 4"s,
      " 5"s,
      " 6"s,
      " 7"s,
  });
  vec4 = vec4 + vec4;

  stub();

  return 0;
}
