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

using TypesToTest =
    ::testing::Types<PathValidator<graph::MockGraphProvider,
                                   PathStore<graph::MockGraphProvider::Step>,
                                   VertexUniquenessLevel::NONE>,
                     PathValidator<graph::MockGraphProvider,
                                   PathStore<graph::MockGraphProvider::Step>,
                                   VertexUniquenessLevel::PATH>,
                     PathValidator<graph::MockGraphProvider,
                                   PathStore<graph::MockGraphProvider::Step>,
                                   VertexUniquenessLevel::GLOBAL>>;

template<class ValidatorType>
class PathValidatorTest : public ::testing::Test {
 protected:
  graph::MockGraph mockGraph;
  mocks::MockAqlServer _server{true};

  std::shared_ptr<arangodb::aql::Query> _query{_server.createFakeQuery()};

 private:
  arangodb::GlobalResourceMonitor _global{};
  arangodb::ResourceMonitor _resourceMonitor{_global};

  std::unique_ptr<graph::MockGraphProvider> _provider;

  PathStore<Step> _pathStore{_resourceMonitor};
  StringHeap _heap{_resourceMonitor, 4096};

  // Expression Parts
  arangodb::transaction::Methods _trx{_query->newTrxContext()};
  aql::Ast* _ast{_query->ast()};
  aql::Variable _tmpVar{"tmp", 0, false};
  aql::AstNode* _varNode{::InitializeReference(*_ast, _tmpVar)};

  arangodb::aql::AqlFunctionsInternalCache _functionsCache{};
  arangodb::aql::FixedVarExpressionContext _expressionContext{_trx, *_query,
                                                              _functionsCache};
  PathValidatorOptions _opts{&_tmpVar, _expressionContext};

 protected:
  VertexUniquenessLevel getVertexUniquness() {
    if constexpr (std::is_same_v<
                      ValidatorType,
                      PathValidator<graph::MockGraphProvider, PathStore<Step>,
                                    VertexUniquenessLevel::NONE>>) {
      return VertexUniquenessLevel::NONE;
    } else if constexpr (std::is_same_v<
                             ValidatorType,
                             PathValidator<graph::MockGraphProvider,
                                           PathStore<Step>,
                                           VertexUniquenessLevel::PATH>>) {
      return VertexUniquenessLevel::PATH;
    } else {
      return VertexUniquenessLevel::GLOBAL;
    }
  }

  PathStore<Step>& store() { return _pathStore; }

  aql::Variable const* tmpVar() { return &_tmpVar; }

  aql::Query* query() { return _query.get(); }

  ValidatorType testee() {
    ensureProvider();
    return ValidatorType{*_provider.get(), this->store(), _opts};
  }

  Step startPath(size_t id) {
    ensureProvider();
    auto base = mockGraph.vertexToId(id);
    HashedStringRef ref{base.c_str(), static_cast<uint32_t>(base.length())};
    auto hStr = _heap.registerString(ref);
    return _provider->startVertex(hStr);
  }

  // Get and modify the options used in the Validator testee().
  // Make sure to Modify them before calling testee() method.
  PathValidatorOptions& options() { return _opts; }

  std::vector<Step> expandPath(Step previous) {
    // We at least have called startPath before, this ensured the provider
    TRI_ASSERT(_provider != nullptr);
    size_t prev = _pathStore.append(previous);
    std::vector<Step> result;
    _provider->expand(previous, prev,
                      [&result](Step s) { result.emplace_back(s); });
    return result;
  }

  // Add a path defined by the given vector. We start a first() and end in
  // last()
  void addPath(std::vector<size_t> path) {
    // This function can only add paths of length 1 or more
    TRI_ASSERT(path.size() >= 2);
    for (size_t i = 0; i < path.size() - 1; ++i) {
      mockGraph.addEdge(path.at(i), path.at(i + 1));
    }
  }

  /*
   * generates a condition #TMP._key == '<toMatch>'
   */
  std::unique_ptr<aql::Expression> conditionKeyMatches(
      std::string const& toMatch) {
    auto expectedKey =
        _ast->createNodeValueString(toMatch.c_str(), toMatch.length());
    auto keyAccess = _ast->createNodeAttributeAccess(
        _varNode, StaticStrings::KeyString.c_str(),
        StaticStrings::KeyString.length());
    // This condition cannot be fulfilled
    auto condition = _ast->createNodeBinaryOperator(
        aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ, keyAccess, expectedKey);
    return std::make_unique<aql::Expression>(_ast, condition);
  }

 private:
  void ensureProvider() {
    if (_provider == nullptr) {
      _provider = std::make_unique<graph::MockGraphProvider>(
          *_query.get(),
          graph::MockGraphProviderOptions{
              mockGraph,
              graph::MockGraphProviderOptions::LooseEndBehaviour::NEVER, false},
          _resourceMonitor);
    }
  }
};

TYPED_TEST_CASE(PathValidatorTest, TypesToTest);

TYPED_TEST(PathValidatorTest,
           it_should_honor_uniqueness_on_single_path_first_duplicate) {
  // We add a loop that ends in the start vertex (0) again.
  this->addPath({0, 1, 2, 3, 0});
  auto validator = this->testee();

  Step s = this->startPath(0);
  {
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
  }
  // The next 3 steps are good to take.
  for (size_t i = 0; i < 3; ++i) {
    auto neighbors = this->expandPath(s);
    ASSERT_EQ(neighbors.size(), 1)
        << "Not enough connections after step " << s.getVertexIdentifier();
    s = neighbors.at(0);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
  }

  // Now we move to the duplicate vertex
  {
    auto neighbors = this->expandPath(s);
    ASSERT_EQ(neighbors.size(), 1);
    s = neighbors.at(0);
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

TYPED_TEST(PathValidatorTest,
           it_should_honor_uniqueness_on_single_path_last_duplicate) {
  // We add a loop that loops on the last vertex(3).
  this->addPath({0, 1, 2, 3, 3});
  auto validator = this->testee();

  Step s = this->startPath(0);
  {
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
  }
  // The next 3 steps are good to take.
  for (size_t i = 0; i < 3; ++i) {
    auto neighbors = this->expandPath(s);
    ASSERT_EQ(neighbors.size(), 1)
        << "Not enough connections after step " << s.getVertexIdentifier();
    s = neighbors.at(0);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
  }

  // Now we move to the duplicate vertex
  {
    auto neighbors = this->expandPath(s);
    ASSERT_EQ(neighbors.size(), 1);
    s = neighbors.at(0);
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

TYPED_TEST(PathValidatorTest,
           it_should_honor_uniqueness_on_single_path_interior_duplicate) {
  // We add a loop that loops on the last vertex(2).
  this->addPath({0, 1, 2, 3, 2});
  auto validator = this->testee();

  Step s = this->startPath(0);
  {
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
  }
  // The next 3 steps are good to take.
  for (size_t i = 0; i < 3; ++i) {
    auto neighbors = this->expandPath(s);
    ASSERT_EQ(neighbors.size(), 1)
        << "Not enough connections after step " << s.getVertexIdentifier();
    s = neighbors.at(0);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
  }

  // Now we move to the duplicate vertex
  {
    auto neighbors = this->expandPath(s);
    ASSERT_EQ(neighbors.size(), 1);
    s = neighbors.at(0);
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

TYPED_TEST(PathValidatorTest,
           it_should_honor_uniqueness_on_global_paths_last_duplicate) {
  // We add a two paths, that share the same start and end vertex (3)
  this->addPath({0, 1, 2, 3});
  this->addPath({0, 4, 5, 3});

  auto validator = this->testee();

  Step s = this->startPath(0);
  {
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
  }

  auto branch = this->expandPath(s);
  // 1 and 4, we do not care on the ordering.
  ASSERT_EQ(branch.size(), 2);
  {
    {
      // Test the branch vertex itself
      s = branch.at(0);
      auto res = validator.validatePath(s);
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    }
    // The first branch is good until the end
    for (size_t i = 0; i < 2; ++i) {
      auto neighbors = this->expandPath(s);
      ASSERT_EQ(neighbors.size(), 1)
          << "Not enough connections after step " << s.getVertexIdentifier();
      s = neighbors.at(0);
      auto res = validator.validatePath(s);
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    }
  }
  {
    // The second branch is good but for the last vertex
    {
      // Test the branch vertex itself
      s = branch.at(1);
      auto res = validator.validatePath(s);
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    }
    for (size_t i = 0; i < 1; ++i) {
      auto neighbors = this->expandPath(s);
      ASSERT_EQ(neighbors.size(), 1)
          << "Not enough connections after step " << s.getVertexIdentifier();
      s = neighbors.at(0);
      auto res = validator.validatePath(s);
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    }

    // Now we move to the duplicate vertex
    {
      auto neighbors = this->expandPath(s);
      ASSERT_EQ(neighbors.size(), 1);
      s = neighbors.at(0);
      auto res = validator.validatePath(s);

      if (this->getVertexUniquness() != VertexUniquenessLevel::GLOBAL) {
        // The vertex is visited twice, but not on same path.
        // As long as we are not GLOBAL this is okay.
        // No uniqueness check, take the vertex
        EXPECT_FALSE(res.isFiltered());
        EXPECT_FALSE(res.isPruned());
      } else {
        // With GLOBAL uniqueness this vertex is illegal
        EXPECT_TRUE(res.isFiltered());
        EXPECT_TRUE(res.isPruned());
      }
    }
  }
}

TYPED_TEST(PathValidatorTest,
           it_should_honor_uniqueness_on_global_paths_interior_duplicate) {
  // We add a two paths, that share the same start and end vertex (3)
  this->addPath({0, 1, 2, 3});
  this->addPath({0, 4, 5, 1});

  auto validator = this->testee();

  Step s = this->startPath(0);
  {
    auto res = validator.validatePath(s);
    // The start vertex is always valid
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
  }

  auto branch = this->expandPath(s);
  // 1 and 4, we do need to care on the ordering, this is right now guaranteed.
  // If this test fails at any point in time, we can add some code here that
  // ensures that we first visit Vertex 1, then Vertex 4
  ASSERT_EQ(branch.size(), 2);
  {
    // The first branch is good until the end
    {
      // Test the branch vertex itself
      s = branch.at(0);
      auto res = validator.validatePath(s);
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    }
    for (size_t i = 0; i < 2; ++i) {
      auto neighbors = this->expandPath(s);
      ASSERT_EQ(neighbors.size(), 1)
          << "Not enough connections after step " << s.getVertexIdentifier();
      s = neighbors.at(0);
      auto res = validator.validatePath(s);
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    }
  }
  {
    // The second branch is good but for the last vertex
    {
      // Test the branch vertex itself
      s = branch.at(1);
      auto res = validator.validatePath(s);
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    }
    for (size_t i = 0; i < 1; ++i) {
      auto neighbors = this->expandPath(s);
      ASSERT_EQ(neighbors.size(), 1)
          << "Not enough connections after step " << s.getVertexIdentifier();
      s = neighbors.at(0);
      auto res = validator.validatePath(s);
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    }

    // Now we move to the duplicate vertex
    {
      auto neighbors = this->expandPath(s);
      ASSERT_EQ(neighbors.size(), 1);
      s = neighbors.at(0);
      auto res = validator.validatePath(s);

      if (this->getVertexUniquness() != VertexUniquenessLevel::GLOBAL) {
        // The vertex is visited twice, but not on same path.
        // As long as we are not GLOBAL this is okay.
        // No uniqueness check, take the vertex
        EXPECT_FALSE(res.isFiltered());
        EXPECT_FALSE(res.isPruned());
      } else {
        // With GLOBAL uniqueness this vertex is illegal
        EXPECT_TRUE(res.isFiltered());
        EXPECT_TRUE(res.isPruned());
      }
    }
  }
}

TYPED_TEST(PathValidatorTest, it_should_test_an_all_vertices_condition) {
  this->addPath({0, 1});
  std::string keyToMatch = "1";

  auto expression = this->conditionKeyMatches(keyToMatch);
  auto& opts = this->options();
  opts.setAllVerticesExpression(std::move(expression));
  auto validator = this->testee();
  {
    // Testing x._key == "1" with `{_key: "1"} => Should succeed
    Step s = this->startPath(1);
    auto res = validator.validatePath(s);
    EXPECT_FALSE(res.isFiltered());
    EXPECT_FALSE(res.isPruned());
  }

  // we start a new path, so reset the uniqueness checks
  validator.reset();

  {
    // Testing x._key == "1" with `{_key: "0"} => Should fail
    Step s = this->startPath(0);
    {
      auto res = validator.validatePath(s);
      EXPECT_TRUE(res.isFiltered());
      EXPECT_TRUE(res.isPruned());
    }

    // Testing condition on level 1 (not start)
    auto neighbors = this->expandPath(s);
    ASSERT_EQ(neighbors.size(), 1);
    s = neighbors.at(0);
    {
      // Testing x._key == "1" with `{_key: "1"} => Should succeed
      auto res = validator.validatePath(s);
      EXPECT_FALSE(res.isFiltered());
      EXPECT_FALSE(res.isPruned());
    }
  }
}

}  // namespace graph_path_validator_test
}  // namespace tests
}  // namespace arangodb
