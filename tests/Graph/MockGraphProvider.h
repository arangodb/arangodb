////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////
#ifndef TESTS_MOCK_GRAPH_PROVIDER_H
#define TESTS_MOCK_GRAPH_PROVIDER_H

#include <vector>

namespace arangodb {

namespace futures {
template <typename T>
class Future;
}

namespace tests {
namespace graph {

class MockGraphProvider {
 public:
  struct Step {};

  MockGraphProvider() {}
  ~MockGraphProvider() {}

  auto fetch(std::vector<Step> const& looseEnds) -> futures::Future<std::vector<Step>>;

 private:
};
}  // namespace graph
}  // namespace tests
}  // namespace arangodb

#endif