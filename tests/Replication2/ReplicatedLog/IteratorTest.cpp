////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "Replication2/ReplicatedLog/LogCommon.h"

using namespace arangodb;
using namespace arangodb::replication2;

namespace arangodb::replication2::test {
template <typename I, typename Value = typename std::iterator_traits<I>::value_type>
struct ContainerIterator : TypedLogIterator<Value> {
  ContainerIterator(I begin, I end) : _current(begin), _end(end) {}

  auto next() -> std::optional<Value> override {
    if (_current != _end) {
      auto it = _current;
      ++_current;
      return *it;
    }
    return std::nullopt;
  }

 private:
  I _current;
  I _end;
};

}  // namespace arangodb::replication2::test

TEST(ReplicationIteratorTest, range_base_for_loop) {
  auto vec = std::vector{1, 2, 3, 4, 5, 6};
  auto citer = test::ContainerIterator(vec.begin(), vec.end());

  int i = 1;
  for (auto const& v : citer) {
    EXPECT_EQ(i, v);
    i += 1;
  }
}
