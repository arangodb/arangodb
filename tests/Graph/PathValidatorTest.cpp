////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "../Mocks/Servers.h"

#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/StringHeap.h"
#include "Basics/StringUtils.h"
#include "Graph/PathManagement/PathStore.cpp"
#include "Graph/PathManagement/PathValidator.cpp"
#include "Graph/Providers/BaseStep.h"
#include "Graph/Types/UniquenessLevel.h"

#include "./MockGraph.h"
#include "./MockGraphProvider.h"

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::velocypack;

namespace {
aql::AstNode* InitializeReference(aql::Ast& ast, aql::Variable& var) {
  ast.scopes()->start(aql::ScopeType::AQL_SCOPE_MAIN);
  ast.scopes()->addVariable(&var);
  aql::AstNode* a = ast.createNodeReference("tmp");
  ast.scopes()->endCurrent();
  return a;
}
}  // namespace

namespace arangodb {
namespace tests {
namespace graph_path_validator_test {

static_assert(GTEST_HAS_TYPED_TEST, "We need typed tests for the following:");

using Step = typename graph::MockGraphProvider::Step;

using TypesToTest = ::testing::Types<
    PathValidator<graph::MockGraphProvider, PathStore<graph::MockGraphProvider::Step>, VertexUniquenessLevel::NONE>,
    PathValidator<graph::MockGraphProvider, PathStore<graph::MockGraphProvider::Step>, VertexUniquenessLevel::PATH>,
    PathValidator<graph::MockGraphProvider, PathStore<graph::MockGraphProvider::Step>, VertexUniquenessLevel::GLOBAL>>;

template <class ValidatorType>
class PathValidatorTest : public ::testing::Test {
 protected:
  graph::MockGraph mockGraph;
  mocks::MockAqlServer _server{true};

  std::unique_ptr<arangodb::aql::Query> _query{_server.createFakeQuery()};

 private:
  arangodb::GlobalResourceMonitor _global{};
  arangodb::ResourceMonitor _resourceMonitor{_global};

  graph::MockGraphProvider _provider{
      *_query.get(),
      graph::MockGraphProviderOptions{mockGraph, graph::MockGraphProviderOptions::LooseEndBehaviour::NEVER,
                                      false},
      _resourceMonitor};

  PathStore<Step> _pathStore{_resourceMonitor};
  StringHeap _heap{_resourceMonitor, 4096};

 protected:
  // Expression Parts
  aql::Ast ast{*_query.get()};
  aql::Variable tmpVar{"tmp", 0, false};
  aql::AstNode* varNode{::InitializeReference(ast, tmpVar)};

 protected:
  VertexUniquenessLevel getVertexUniquness() {
    if constexpr (std::is_same_v<ValidatorType, PathValidator<graph::MockGraphProvider, PathStore<Step>, VertexUniquenessLevel::NONE>>) {
      return VertexUniquenessLevel::NONE;
    } else if constexpr (std::is_same_v<ValidatorType, PathValidator<graph::MockGraphProvider, PathStore<Step>, VertexUniquenessLevel::PATH>>) {
      return VertexUniquenessLevel::PATH;
    } else {
      return VertexUniquenessLevel::GLOBAL;
    }
  }

  PathStore<Step>& store() { return _pathStore; }

  ValidatorType testee(PathValidatorOptions opts = PathValidatorOptions{}) {
    return ValidatorType{this->_provider, this->store(), std::move(opts)};
  }

  Step makeStep(size_t id, size_t previous) {
    // TODO this needs to be fully shifted to the provider
    std::string idStr = mockGraph.vertexToId(id);
    HashedStringRef hStr(idStr.data(), static_cast<uint32_t>(idStr.length()));
    if (previous == std::numeric_limits<size_t>::max()) {
      return _provider.startVertex(hStr);
    }
    return Step(_heap.registerString(hStr), previous);
  }
};

TYPED_TEST_CASE(PathValidatorTest, TypesToTest);

TYPED_TEST(PathValidatorTest, it_should_honor_uniqueness_on_single_path_first_duplicate) {
  auto&& ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  {
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 0);
  }

  // We add a loop that ends in the start vertex (0) again.
  {
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s = this->makeStep(2, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // Add duplicate vertex on Path
  {
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    if (this->getVertexUniquness() == VertexUniquenessLevel::NONE) {
      // No uniqueness check, take the vertex
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With PATH or GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TYPED_TEST(PathValidatorTest, it_should_honor_uniqueness_on_single_path_last_duplicate) {
  auto&& ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  {
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 0);
  }
  // We add a loop on the last vertex of the path (3)
  {
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s = this->makeStep(2, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // Add duplicate vertex on Path
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    if (this->getVertexUniquness() == VertexUniquenessLevel::NONE) {
      // No uniqueness check, take the vertex
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With PATH or GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TYPED_TEST(PathValidatorTest, it_should_honor_uniqueness_on_single_path_interior_duplicate) {
  auto&& ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  {
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 0);
  }
  // We add a loop that ends in one interior vertex (2) again.
  {
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s = this->makeStep(2, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // Add duplicate vertex on Path
  {
    Step s = this->makeStep(2, lastIndex);
    auto res = validator.validatePath(s);
    if (this->getVertexUniquness() == VertexUniquenessLevel::NONE) {
      // No uniqueness check, take the vertex
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With PATH or GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TYPED_TEST(PathValidatorTest, it_should_honor_uniqueness_on_global_paths_last_duplicate) {
  auto&& ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  {
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 0);
  }
  // We add two paths, each without a loop.
  // Both paths share a common vertex besides the start.

  // First path 0 -> 1 -> 2 -> 3
  {
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s = this->makeStep(2, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // First path 0 -> 4 -> 5
  lastIndex = 0;
  {
    Step s = this->makeStep(4, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 4);
  }
  {
    Step s = this->makeStep(5, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 5);
  }

  // Add duplicate vertex (3) which is the last on the first path
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    if (this->getVertexUniquness() != VertexUniquenessLevel::GLOBAL) {
      // The vertex is visited twice, but not on same path.
      // As long as we are not GLOBAL this is okay.
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TYPED_TEST(PathValidatorTest, it_should_honor_uniqueness_on_global_paths_interior_duplicate) {
  auto&& ps = this->store();
  auto validator = this->testee();

  size_t lastIndex = std::numeric_limits<size_t>::max();
  {
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 0);
  }
  // We add two paths, each without a loop.
  // Both paths share a common vertex besides the start.

  // First path 0 -> 1 -> 2 -> 3
  {
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 1);
  }
  {
    Step s = this->makeStep(2, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 2);
  }
  {
    Step s = this->makeStep(3, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 3);
  }

  // First path 0 -> 4 -> 5
  lastIndex = 0;
  {
    Step s = this->makeStep(4, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 4);
  }
  {
    Step s = this->makeStep(5, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
    lastIndex = ps.append(std::move(s));
    EXPECT_EQ(lastIndex, 5);
  }

  // Add duplicate vertex (1) which is interior to the first path
  {
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    if (this->getVertexUniquness() != VertexUniquenessLevel::GLOBAL) {
      // The vertex is visited twice, but not on same path.
      // As long as we are not GLOBAL this is okay.
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    } else {
      // With GLOBAL uniqueness this vertex is illegal
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }
  }
}

TYPED_TEST(PathValidatorTest, it_should_test_an_all_vertices_condition) {
  this->mockGraph.addEdge(0, 1);
  PathValidatorOptions opts(*(this->_query.get()), &this->tmpVar);
  std::string matchedKey{"1"};
  auto expectedKey =
      this->ast.createNodeValueString(matchedKey.c_str(), matchedKey.length());
  auto keyAccess =
      this->ast.createNodeAttributeAccess(this->varNode,
                                          StaticStrings::KeyString.c_str(),
                                          StaticStrings::KeyString.length());
  // This condition cannot be fulfilled
  auto condition = this->ast.createNodeBinaryOperator(aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ,
                                                      keyAccess, expectedKey);
  auto expression = std::make_unique<aql::Expression>(&this->ast, condition);
  opts.setAllVerticesExpression(std::move(expression));
  auto validator = this->testee(std::move(opts));
  {
    // Testing x._key == "1" with `{_key: "0"} => Should fail
    size_t lastIndex = std::numeric_limits<size_t>::max();
    Step s = this->makeStep(0, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_TRUE(res.isFiltered());
    EXPECT_TRUE(res.isPruned());
  }
  {
    // Testing x._key == "1" with `{_key: "1"} => Should succeed
    size_t lastIndex = std::numeric_limits<size_t>::max();
    Step s = this->makeStep(1, lastIndex);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
  }
}

}  // namespace graph_path_validator_test
}  // namespace tests
}  // namespace arangodb
