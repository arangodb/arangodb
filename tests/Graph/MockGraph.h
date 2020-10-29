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

#include <numeric>
#include <vector>

namespace arangodb {
namespace tests {
namespace graph {

class MockGraph {
 public:
  struct EdgeDef {
    EdgeDef(size_t from, size_t to, double weight)
        : _from(from), _to(to), _weight(weight){};
    size_t _from;
    size_t _to;
    double _weight;
  };

 public:
  MockGraph() {}
  ~MockGraph() {}

  void addEdge(size_t from, size_t to, double weight = 1.0) {
    _edges.emplace_back(EdgeDef{from, to, weight});
  }

  auto edges() const -> std::vector<EdgeDef> const& {
    return _edges;
  }

 private:
  std::vector<EdgeDef> _edges;
};
}
}
}

#endif