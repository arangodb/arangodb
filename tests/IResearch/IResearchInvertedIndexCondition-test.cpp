////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////
#include "common.h"
#include "gtest/gtest.h"
#include "IResearch/common.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/ExpressionContextMock.h"
#include "IResearch/IResearchFilterFactory.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/Methods/Collections.h"
#include "Basics/StaticStrings.h"

class IResearchInvertedIndexConditionTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;
  std::shared_ptr<arangodb::LogicalCollection> _collection;

 protected:
  IResearchInvertedIndexConditionTest() {
    arangodb::tests::init();
   
    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(testDBInfo(server.server()), _vocbase);
    
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(*_vocbase, options,
                                                 arangodb::tests::AnalyzerCollectionName,
                                                 false, _collection);
  }
  VPackBuilder getPropertiesSlice(arangodb::IndexId iid, std::vector<std::string>& fields) {
    VPackBuilder vpack;

    return vpack;
  }

  ~IResearchInvertedIndexConditionTest() {
    _collection.reset();
  }

  arangodb::LogicalCollection& collection() {return *_collection;}
  TRI_vocbase_t& vocbase() { return *_vocbase; }
};  // IResearchFilterSetup

TEST_F(IResearchInvertedIndexConditionTest, test_conjunction) {

  std::string queryString = "FOR d IN test FILTER d.a = 'value' ";
  std::string const refName = "d";
  std::vector<std::string> fields = {"a"};
  arangodb::aql::ExpressionContext* exprCtx = nullptr;
  arangodb::IndexId id(1);
  arangodb::iresearch::IResearchInvertedIndex Index(id, collection(), getPropertiesSlice(id, fields).slice());


  auto ctx = std::make_shared<arangodb::transaction::StandaloneContext>(vocbase());
  arangodb::aql::Query query(ctx, arangodb::aql::QueryString(queryString),
                             nullptr);

  auto const parseResult = query.parse();
  ASSERT_TRUE(parseResult.result.ok());

  auto* ast = query.ast();
  ASSERT_TRUE(ast);

  auto* root = ast->root();
  ASSERT_TRUE(root);

  // find first FILTER node
  arangodb::aql::AstNode* filterNode = nullptr;
  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto* node = root->getMemberUnchecked(i);
    ASSERT_TRUE(node);

    if (arangodb::aql::NODE_TYPE_FILTER == node->type) {
      filterNode = node;
      break;
    }
  }
  ASSERT_TRUE(filterNode);

  // find referenced variable
  auto* allVars = ast->variables();
  ASSERT_TRUE(allVars);
  arangodb::aql::Variable* ref = nullptr;
  for (auto entry : allVars->variables(true)) {
    if (entry.second == refName) {
      ref = allVars->getVariable(entry.first);
      break;
    }
  }
  ASSERT_TRUE(ref);

  // optimization time
  {
    arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase()),
                                        {}, {}, {}, arangodb::transaction::Options());
    
    auto* mockCtx = dynamic_cast<ExpressionContextMock*>(exprCtx);
    if (mockCtx) {  // simon: hack to make expression context work again
      mockCtx->setTrx(&trx);
    }

    arangodb::iresearch::QueryContext const ctx{&trx, nullptr, nullptr, nullptr, nullptr, ref};
   
  }
}
