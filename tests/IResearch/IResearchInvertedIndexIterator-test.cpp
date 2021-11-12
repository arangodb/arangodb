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
#include "Aql/Projections.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchInvertedIndex.h"
#include "IResearch/ExpressionContextMock.h"
#include "IResearch/IResearchFilterFactory.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/Methods/Collections.h"
#include "Basics/StaticStrings.h"

namespace {
std::vector<std::vector<std::string>> EMPTY_STORED_FIELDS{};
}

class IResearchInvertedIndexIteratorTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  

 private:
  arangodb::tests::mocks::MockAqlServer _server{false};
  std::shared_ptr<arangodb::LogicalCollection> _analyzers;
  std::shared_ptr<arangodb::LogicalCollection> _collection;
  std::shared_ptr<arangodb::iresearch::IResearchInvertedIndex> _index;
  TRI_vocbase_t* _vocbase{nullptr};
 protected:
  IResearchInvertedIndexIteratorTest() {
    arangodb::tests::init();
    _server.addFeature<arangodb::FlushFeature>(false);
    _server.startFeatures();
    auto& dbFeature = _server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(testDBInfo(_server.server()), _vocbase);

    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(*_vocbase, options,
                                                 arangodb::tests::AnalyzerCollectionName,
                                                 false, _analyzers);

    auto createCollection = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
    _collection = vocbase().createCollection(createCollection->slice());
    EXPECT_TRUE(_collection);
    arangodb::IndexId id(1);
    arangodb::iresearch::InvertedIndexFieldMeta meta;
    std::string errorField;
    std::vector<std::vector<std::string>> storedFields = {{"a", "b"}};
    std::vector<std::string> fields = {"a", "b"};
    EXPECT_TRUE(meta.init(_server.server(),
                          getPropertiesSlice(id, fields, storedFields).slice(),
                          false, errorField, _vocbase->name()));
    _index = std::make_shared<arangodb::iresearch::IResearchInvertedIndex>(id, *_collection, std::move(meta));
    EXPECT_TRUE(_index);
    EXPECT_TRUE(_index->init().ok());

    // now populate the docs
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{_collection->name()};
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase()),
                                       EMPTY, collections, EMPTY,
                                       arangodb::transaction::Options());
     auto doc0 = arangodb::velocypack::Parser::fromJson(
        R"("{ "a":"1", "b":"2"  }")");
     arangodb::LocalDocumentId doc0Id{1};
     _index->insert<arangodb::iresearch::InvertedIndexFieldIterator, 
                    arangodb::iresearch::InvertedIndexFieldMeta>(trx, doc0Id,
                                                                 doc0->slice(), _index->_meta);
  }

  VPackBuilder getPropertiesSlice(arangodb::IndexId iid,
                                  std::vector<std::string> const& fields,
                                  std::vector<std::vector<std::string>> const& storedFields = EMPTY_STORED_FIELDS) {
    VPackBuilder vpack;
    {
      VPackObjectBuilder obj(&vpack);
      vpack.add(arangodb::StaticStrings::IndexId, VPackValue(iid.id()));
      vpack.add(arangodb::StaticStrings::IndexType, VPackValue("inverted"));

      //FIXME: maybe this should be set by index internally ?
      vpack.add(arangodb::StaticStrings::IndexUnique, VPackValue(false));
      vpack.add(arangodb::StaticStrings::IndexSparse, VPackValue(true));

      {
        VPackArrayBuilder arrayFields(&vpack, arangodb::StaticStrings::IndexFields);
        for (auto const& f : fields) {
          vpack.add(VPackValue(f));
        }
      }
      if (!storedFields.empty())
      {
        VPackArrayBuilder arrayFields(&vpack, "storedValues");
        for (auto const& f : storedFields) {
          VPackArrayBuilder arrayFields(&vpack);
          for (auto const& s : f) {
            vpack.add(VPackValue(s));
          }
        }
      }
    }
    return vpack;
  }

  ~IResearchInvertedIndexIteratorTest() {
    _collection.reset();
  }

  //void assertProjectionsCoverageSuccess(
  //  std::vector<std::vector<std::string>> const& storedFields,
  //  std::vector<arangodb::aql::AttributeNamePath> const& attributes,
  //  arangodb::aql::Projections const& expected) {
  //  arangodb::aql::Projections projections(attributes);
  //  arangodb::IndexId id(1);
  //  arangodb::iresearch::InvertedIndexFieldMeta meta;
  //  std::string errorField;
  //  std::vector<std::string> fields = {"a"};
  //  ASSERT_TRUE(meta.init(_server.server(),
  //                        getPropertiesSlice(id, fields, storedFields).slice(),
  //                        false, errorField, _vocbase->name()));
  //  arangodb::iresearch::IResearchInvertedIndex Index(id, *_collection, std::move(meta));
  //  ASSERT_TRUE(Index.covers(projections));
  //  ASSERT_EQ(expected.size(), projections.size());
  //  for (size_t i = 0; i < expected.size(); ++i) {
  //    ASSERT_EQ(expected[i].path, projections[i].path);
  //    ASSERT_EQ(expected[i].coveringIndexCutoff, projections[i].coveringIndexCutoff);
  //    ASSERT_EQ(expected[i].coveringIndexPosition, projections[i].coveringIndexPosition);
  //  }
  //}

  //void estimateFilterCondition(
  //    std::string const& queryString, std::vector<std::string> const& fields,
  //    arangodb::Index::FilterCosts const& expectedCosts,
  //    arangodb::aql::ExpressionContext* exprCtx = nullptr,
  //    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
  //    std::string const& refName = "d") {
  //  SCOPED_TRACE(testing::Message("estimateFilterCondition failed for query:<")
  //    << queryString << "> Expected support:" << expectedCosts.supportsCondition);
  //  arangodb::IndexId id(1);
  //  arangodb::iresearch::InvertedIndexFieldMeta meta;
  //  std::string errorField;
  //  meta.init(_server.server(), getPropertiesSlice(id, fields).slice(), false, errorField, _vocbase->name());
  //  auto indexFields =arangodb::iresearch::IResearchInvertedIndex::fields(meta);
  //  arangodb::iresearch::IResearchInvertedIndex Index(id, *_collection, std::move(meta));


  //  auto ctx = std::make_shared<arangodb::transaction::StandaloneContext>(vocbase());
  //  auto query = arangodb::aql::Query::create(ctx, arangodb::aql::QueryString(queryString),
  //                                            bindVars);

  //  ASSERT_NE(query.get(), nullptr);
  //  auto const parseResult = query->parse();
  //  ASSERT_TRUE(parseResult.result.ok());

  //  auto* ast = query->ast();
  //  ASSERT_TRUE(ast);

  //  auto* root = ast->root();
  //  ASSERT_TRUE(root);

  //  // find first FILTER node
  //  arangodb::aql::AstNode* filterNode = nullptr;
  //  for (size_t i = 0; i < root->numMembers(); ++i) {
  //    auto* node = root->getMemberUnchecked(i);
  //    ASSERT_TRUE(node);

  //    if (arangodb::aql::NODE_TYPE_FILTER == node->type) {
  //      filterNode = node;
  //      break;
  //    }
  //  }
  //  ASSERT_TRUE(filterNode);

  //  // find referenced variable
  //  auto* allVars = ast->variables();
  //  ASSERT_TRUE(allVars);
  //  arangodb::aql::Variable* ref = nullptr;
  //  for (auto entry : allVars->variables(true)) {
  //    if (entry.second == refName) {
  //      ref = allVars->getVariable(entry.first);
  //      break;
  //    }
  //  }
  //  ASSERT_TRUE(ref);

  //  // optimization time
  //  {
  //    arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase()),
  //                                        {}, {}, {}, arangodb::transaction::Options());
  //    auto* mockCtx = dynamic_cast<ExpressionContextMock*>(exprCtx);
  //    if (mockCtx) {
  //      mockCtx->setTrx(&trx);
  //    }

  //    arangodb::iresearch::QueryContext const ctx{&trx, nullptr, nullptr, mockCtx, nullptr, ref};
  //    auto costs = Index.supportsFilterCondition(id, indexFields, {}, filterNode, ref, 0);
  //    ASSERT_EQ(expectedCosts.supportsCondition, costs.supportsCondition);
  //  }
  //  // runtime is not intended - we must decide during optimize time!
  //}

  arangodb::LogicalCollection& collection() {return *_collection;}
  TRI_vocbase_t& vocbase() { return *_vocbase; }
};  // IResearchFilterSetup

TEST_F(IResearchInvertedIndexIteratorTest, test_skip) {

}
