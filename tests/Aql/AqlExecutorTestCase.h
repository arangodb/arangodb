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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef TESTS_AQL_EXECUTORTESTCASE_H
#define TESTS_AQL_EXECUTORTESTCASE_H

#include "gtest/gtest.h"

#include "ExecutorTestHelper.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/ExecutionNode.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"

#include "Mocks/Servers.h"

using namespace arangodb::aql;
using namespace arangodb::tests::aql;

namespace arangodb {
namespace tests {
namespace aql {

/**
 * @brief Base class for ExecutorTests in Aql.
 *        It will provide a test server, including
 *        an AqlQuery, as well as the ability to generate
 *        Dummy ExecutionNodes.
 *
 * @tparam enableQueryTrace Enable Aql Profile Trace logging
 */
template <bool enableQueryTrace = false>
class AqlExecutorTestCase : public ::testing::Test {
 public:
  // Creating a server instance costs a lot of time, so do it only once.
  // Note that newer version of gtest call these SetUpTestSuite/TearDownTestSuite
  static void SetUpTestCase() {
    _server = std::make_unique<mocks::MockAqlServer>();
  }

  static void TearDownTestCase() { _server.reset(); }

 protected:
  AqlExecutorTestCase();
  virtual ~AqlExecutorTestCase();

  auto manager() const -> AqlItemBlockManager&;
  /**
   * @brief Creates and manages a ExecutionNode.
   *        These nodes can be used to create the Executors
   *        Caller does not need to manage the memory.
   *
   * @return ExecutionNode* Pointer to a dummy ExecutionNode. Memory is managed, do not delete.
   */
  auto generateNodeDummy(ExecutionNode::NodeType type = ExecutionNode::NodeType::SINGLETON)
      -> ExecutionNode*;

  auto generateScatterNodeDummy() -> ScatterNode*;

  template <std::size_t inputColumns = 1, std::size_t outputColumns = 1>
  auto makeExecutorTestHelper() -> ExecutorTestHelper<inputColumns, outputColumns> {
    return ExecutorTestHelper<inputColumns, outputColumns>(*fakedQuery, itemBlockManager);
  }

 private:
  std::vector<std::unique_ptr<ExecutionNode>> _execNodes;

 protected:
  // available variables
  static inline std::unique_ptr<mocks::MockAqlServer> _server;
  arangodb::GlobalResourceMonitor global{};
  arangodb::ResourceMonitor monitor{global};
  AqlItemBlockManager itemBlockManager{monitor, SerializationFormat::SHADOWROWS};
  std::unique_ptr<arangodb::aql::Query> fakedQuery;
};

/**
 * @brief Shortcut handle for parameterized AqlExecutorTestCases with param
 *
 * @tparam T The Test Parameter used for gtest.
 * @tparam enableQueryTrace Enable Aql Profile Trace logging
 */
template <typename T, bool enableQueryTrace = false>
class AqlExecutorTestCaseWithParam : public AqlExecutorTestCase<enableQueryTrace>,
                                     public ::testing::WithParamInterface<T> {};

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
#endif
