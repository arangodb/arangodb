////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <gtest/gtest.h>

#include "Basics/VelocyPackHelper.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <string>
#include <string_view>
#include <vector>

namespace arangodb::tests {
namespace detail {

inline std::string childPath(std::string const& path, std::string_view key) {
  return path.empty() ? std::string{key} : path + "." + std::string{key};
}

inline void collectVPackDiffs(velocypack::Slice expected,
                              velocypack::Slice actual, std::string const& path,
                              std::vector<std::string>& diffs) {
  if (expected.isObject() && actual.isObject()) {
    for (auto pair : velocypack::ObjectIterator(expected)) {
      auto const key = pair.key.stringView();
      if (auto const other = actual.get(key); other.isNone()) {
        diffs.push_back(childPath(path, key) + ": missing");
      } else {
        collectVPackDiffs(pair.value, other, childPath(path, key), diffs);
      }
    }
    for (auto pair : velocypack::ObjectIterator(actual)) {
      auto const key = pair.key.stringView();
      if (expected.get(key).isNone()) {
        diffs.push_back(childPath(path, key) + ": unexpected " +
                        pair.value.toJson());
      }
    }
    return;
  }
  if (expected.isArray() && actual.isArray() &&
      expected.length() == actual.length()) {
    for (velocypack::ValueLength i = 0; i < expected.length(); ++i) {
      collectVPackDiffs(expected.at(i), actual.at(i),
                        path + "[" + std::to_string(i) + "]", diffs);
    }
    return;
  }
  if (!basics::VelocyPackHelper::equal(expected, actual, /*useUTF8*/ true)) {
    diffs.push_back((path.empty() ? "<root>" : path) + ": expected " +
                    expected.toJson() + ", actual " + actual.toJson());
  }
}

}  // namespace detail

// reports every divergence at its shallowest path, and unlike
// VelocyPackHelper::equal a missing key is not the same as null
inline ::testing::AssertionResult EqualsVPack(velocypack::Slice expected,
                                              velocypack::Slice actual) {
  std::vector<std::string> diffs;
  detail::collectVPackDiffs(expected, actual, "", diffs);
  if (diffs.empty()) {
    return ::testing::AssertionSuccess();
  }
  auto failure = ::testing::AssertionFailure() << "differs at";
  for (auto const& diff : diffs) {
    failure << "\n  " << diff;
  }
  return failure << "\nexpected " << expected.toJson() << "\n  actual "
                 << actual.toJson();
}

}  // namespace arangodb::tests
