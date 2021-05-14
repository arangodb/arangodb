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
  VPackBuilder getPropertiesSlice(arangodb::IndexId iid, std::vector<std::string> const& fields) {
    VPackBuilder vpack;
    {
      VPackObjectBuilder obj(&vpack);
      vpack.add(arangodb::StaticStrings::IndexId, VPackValue(iid.id()));
      vpack.add(arangodb::StaticStrings::IndexType, VPackValue("search"));
      
      //FIXME: maybe this should be set by index internally ?
      vpack.add(arangodb::StaticStrings::IndexUnique, VPackValue(false));
      vpack.add(arangodb::StaticStrings::IndexSparse, VPackValue(true));

      {
        VPackArrayBuilder arrayFields(&vpack, arangodb::StaticStrings::IndexFields);
        for(auto const& f : fields) {
          vpack.add(VPackValue(f));
        }
      }
    }
    return vpack;
  }

  ~IResearchInvertedIndexConditionTest() {
    _collection.reset();
  }

  void estimateFilterCondition(
      std::string const& queryString, std::vector<std::string> const& fields,
      arangodb::Index::FilterCosts const& expectedCosts,
      std::string const& refName = "d") {
    SCOPED_TRACE(testing::Message("estimateFilterCondition failed for query:<")
      << queryString << "> Expected support:" << expectedCosts.supportsCondition << " Expected num covered:" << expectedCosts.coveredAttributes);
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
    
      // FIXME: uncomment or remove  - after we will be sure if we need to calculate expressions here
      //auto* mockCtx = dynamic_cast<ExpressionContextMock*>(exprCtx);
      //if (mockCtx) {
      //  mockCtx->setTrx(&trx);
      //}

      arangodb::iresearch::QueryContext const ctx{&trx, nullptr, nullptr, nullptr, nullptr, ref};
      auto costs = Index.supportsFilterCondition({}, filterNode, ref, 0);
      ASSERT_EQ(expectedCosts.coveredAttributes, costs.coveredAttributes);
      ASSERT_EQ(expectedCosts.supportsCondition, costs.supportsCondition);
    }

  }

  arangodb::LogicalCollection& collection() {return *_collection;}
  TRI_vocbase_t& vocbase() { return *_vocbase; }
 };  // IResearchFilterSetup

TEST_F(IResearchInvertedIndexConditionTest, test_with_equality) {

  std::string queryString = "FOR d IN test FILTER d.a == 'value' RETURN d ";
  std::vector<std::string> fields = {"a"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  expected.coveredAttributes = 1;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_equality_many_fields) {

  std::string queryString = "FOR d IN test FILTER d.a == 'value' OR d.b == 'value2' AND d.c == 'value3' RETURN d ";
  std::vector<std::string> fields = {"a", "b", "c", "d"};
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  expected.supportsCondition = true;
  expected.coveredAttributes = 3;
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_no_fields) {

  std::string queryString = "FOR d IN test FILTER d.a == 'value' RETURN d ";
  std::vector<std::string> fields = {"b"}; // field a is not indexed :(
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}


TEST_F(IResearchInvertedIndexConditionTest, test_with_no_fields_one_missing) {

  std::string queryString = "FOR d IN test FILTER d.a == 'value' OR d.b == 'c' RETURN d ";
  std::vector<std::string> fields = {"b"}; // field a is not indexed :(
  auto expected = arangodb::Index::FilterCosts::defaultCosts(0);
  estimateFilterCondition(queryString, fields, expected);
}

TEST_F(IResearchInvertedIndexConditionTest, test_with_expression) {

  std::string queryString = "FOR d IN test FILTER d.a == NOOPT('value') RETURN d ";
  std::vector<std::string> fields = {"a"};
  estimateFilterCondition(queryString, fields, arangodb::Index::FilterCosts::defaultCosts(0));
}
