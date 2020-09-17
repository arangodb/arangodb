////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

// test setup
#include "gtest/gtest.h"
#include "../Mocks/Servers.h"
#include "../Mocks/StorageEngineMock.h"
#include "IResearch/common.h"
#include "GraphTestTools.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace graph {

bool checkPath(ShortestPathOptions* spo, ShortestPathResult result, std::vector<std::string> vertices,
                        std::vector<std::pair<std::string, std::string>> edges, std::string& msgs) {
  bool res = true;

  if (result.length() != vertices.size()) return false;

  for (size_t i = 0; i < result.length(); i++) {
    AqlValue vert = result.vertexToAqlValue(spo->cache(), i);
    AqlValueGuard guard{vert, true};
    if (!vert.slice().get(StaticStrings::KeyString).isEqualString(vertices.at(i))) {
      msgs += "expected vertex " + vertices.at(i) + " but found "
        + vert.slice().get(StaticStrings::KeyString).toString() + "\n";
      res = false;
    }
  }

  // First edge is by convention null
  EXPECT_TRUE(result.edgeToAqlValue(spo->cache(), 0).isNull(true));
  for (size_t i = 1; i < result.length(); i++) {
    AqlValue edge = result.edgeToAqlValue(spo->cache(), i);
    AqlValueGuard guard{edge, true};
    if (!edge.slice().get(StaticStrings::FromString).isEqualString(edges.at(i).first) ||
        !edge.slice().get(StaticStrings::ToString).isEqualString(edges.at(i).second)) {
      msgs += "expected edge " + edges.at(i).first + " -> "
                + edges.at(i).second + " but found "
                + edge.slice().get(StaticStrings::FromString).toString() + " -> "
                + edge.slice().get(StaticStrings::ToString).toString() + "\n";
      res = false;
    }
  }
  return res;
}

}
}
}
