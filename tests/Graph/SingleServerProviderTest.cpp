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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "./GraphTestTools.h"
#include "./MockGraph.h"
#include "./MockGraphProvider.h"

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Graph/Providers/SingleServerProvider.h"

#include <velocypack/velocypack-aliases.h>
#include <unordered_set>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::tests::graph;
using namespace arangodb::graph;

namespace arangodb {
namespace tests {
namespace single_server_provider_test {

class SingleServerProviderTest : public ::testing::Test {
 protected:
  // Only used to mock a singleServer
  std::unique_ptr<GraphTestSetup> s{nullptr};
  std::unique_ptr<MockGraphDatabase> singleServer{nullptr};
  std::unique_ptr<arangodb::aql::Query> query{nullptr};
  arangodb::GlobalResourceMonitor _global{};
  arangodb::ResourceMonitor _resourceMonitor{_global};

  std::map<std::string, std::string> _emptyShardMap{};

  SingleServerProviderTest() {}
  ~SingleServerProviderTest() {}

  auto makeProvider(MockGraph const& graph) -> arangodb::graph::SingleServerProvider {
    // Setup code for each provider type
    s = std::make_unique<GraphTestSetup>();
    singleServer =
        std::make_unique<MockGraphDatabase>(s->server, "testVocbase");
    singleServer->addGraph(graph);

    // We now have collections "v" and "e"
    query = singleServer->getQuery("RETURN 1", {"v", "e"});

    auto edgeIndexHandle = singleServer->getEdgeIndexHandle("e");
    auto tmpVar = singleServer->generateTempVar(query.get());
    auto indexCondition = singleServer->buildOutboundCondition(query.get(), tmpVar);

    std::vector<IndexAccessor> usedIndexes{IndexAccessor{edgeIndexHandle, indexCondition, 0}};

    BaseProviderOptions opts(tmpVar, std::move(usedIndexes), _emptyShardMap);
    return {*query.get(), std::move(opts), _resourceMonitor};
  }
};

TEST_F(SingleServerProviderTest, it_must_be_described) {
  ASSERT_TRUE(true);
}

}  // namespace single_server_provider_test
}  // namespace tests
}  // namespace arangodb
