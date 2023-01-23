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
#include "gtest/gtest.h"

//#include "Basics/VelocyPackStringLiteral.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"
#include <Inspection/VPackWithErrorT.h>
// #include <Inspection/StatusT.h>

// Mock includes
#include "MockQuery.h"  // might not be needed
#include "Transaction/StandaloneContext.h"
#include "../../Graph/GraphTestTools.h"
#include "Servers.h"

#include <fmt/core.h>

using namespace arangodb::velocypack;
using namespace arangodb::inspection;

// TEST(Inspection, statust_test) {
//   {
//     auto s = StatusT<int>::ok(15);
//     EXPECT_TRUE(s.ok());
//    EXPECT_EQ(s.get(), 15);
//  }
//
//  {
//    auto s = StatusT<int>::error(Status("error"));
//
//    EXPECT_FALSE(s.ok());
//    EXPECT_EQ(s.error(), "error");
//  }
//}

struct Dummy {
  std::string type;
  size_t id;
};

template<typename Inspector>
auto inspect(Inspector& f, Dummy& x) {
  return f.object(x).fields(f.field("type", x.type), f.field("id", x.id));
}

TEST(Inspection, statust_test_deserialize) {
  auto testSlice = R"({
    "type": "ReturnNode",
    "id": 3
  })"_vpack;

  auto res = deserializeWithErrorT<Dummy>(testSlice);

  ASSERT_TRUE(res.ok()) << fmt::format("Something went wrong: {}",
                                       res.error().error());

  EXPECT_EQ(res->type, "ReturnNode");
  EXPECT_EQ(res->id, 3u);
}

TEST(Inspection, statust_test_deserialize_fail) {
  auto testSlice = R"({
    "type": "ReturnNode",
    "id": 3,
    "fehler": 2
  })"_vpack;

  auto res = deserializeWithErrorT<Dummy>(testSlice);

  ASSERT_FALSE(res.ok()) << fmt::format("Did not detect the error we exepct");

  EXPECT_EQ(res.error().error(), "Found unexpected attribute 'fehler'");
}

TEST(Inspection, arangodb_query_plan_to_inspectables) {
  // TODO: This has been the original suited place for our inspectable plan
  // tests but does not work right now "out of the box" as a lot of components
  // are required to actually run them (e.g. QueryMock)
  /*std::unique_ptr<arangodb::tests::graph::GraphTestSetup> s{nullptr};
  std::unique_ptr<arangodb::tests::graph::MockGraphDatabase>
  singleServer{nullptr}; std::shared_ptr<arangodb::aql::Query> query{nullptr};


  singleServer =
      std::make_unique<arangodb::tests::graph::MockGraphDatabase>(s->server,
  "testVocbase");


  query = singleServer->getQuery("RETURN 1", {"v", "e"});*/

  /*
  std::string queryString = "RETURN 1";
  using MockQuery = arangodb::tests::mocks::MockQuery;
  arangodb::tests::mocks::MockServer server{"PRMR_0001", true};
  auto ctx = std::make_shared<arangodb::transaction::StandaloneContext>(
      server.getSystemDatabase());
  auto fakeQuery = std::make_shared<MockQuery>(ctx, queryString);
*/
}

using namespace arangodb::velocypack;
int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}