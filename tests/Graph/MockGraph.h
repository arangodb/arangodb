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

#ifndef TESTS_MOCK_GRAPH_H
#define TESTS_MOCK_GRAPH_H

#include "Basics/operating-system.h"

#include "Basics/StringUtils.h"

#include <numeric>
#include <vector>

namespace arangodb {
namespace tests {
namespace graph {

class MockGraph {
 public:
  struct EdgeDef {
    EdgeDef(std::string from, std::string to, double weight)
        : _from(from), _to(to), _weight(weight){};
    std::string _from;
    std::string _to;
    double _weight;
  };

 public:
  MockGraph() {}
  ~MockGraph() {}

  void addEdge(std::string from, std::string to, double weight = 1.0) {
    _edges.emplace_back(EdgeDef{from, to, weight});
  }

  void addEdge(size_t from, size_t to, double weight = 1.0) {
    _edges.emplace_back(EdgeDef{basics::StringUtils::itoa(from),
                                basics::StringUtils::itoa(to), weight});
  }

  auto edges() const -> std::vector<EdgeDef> const& { return _edges; }

 private:
  std::vector<EdgeDef> _edges;
};
}  // namespace graph
}  // namespace tests
}  // namespace arangodb

#endif