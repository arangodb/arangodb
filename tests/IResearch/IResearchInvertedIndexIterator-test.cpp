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
#include "Basics/StaticStrings.h"
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
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/Methods/Collections.h"


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
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase()),
                                         EMPTY, collections, EMPTY,
                                         arangodb::transaction::Options());
      trx.begin();
      {
         auto doc = arangodb::velocypack::Parser::fromJson(R"({"a":"1", "b":"2"})");
         arangodb::LocalDocumentId docId{1};
         // MSVC fails to compile if EXPECT_TRUE  is called directly
         auto res = _index->insert<arangodb::iresearch::InvertedIndexFieldIterator, 
                                   arangodb::iresearch::InvertedIndexFieldMeta>(
                                      trx, docId,
                                      doc->slice(), _index->_meta).ok();
         EXPECT_TRUE(res);
      }
      {
         auto doc = arangodb::velocypack::Parser::fromJson(R"({"a":"2", "b":"1"})");
         arangodb::LocalDocumentId docId{2};
         // MSVC fails to compile if EXPECT_TRUE  is called directly
         auto res = _index->insert<arangodb::iresearch::InvertedIndexFieldIterator, 
                                   arangodb::iresearch::InvertedIndexFieldMeta>(
                                      trx, docId,
                                      doc->slice(), _index->_meta).ok();
         EXPECT_TRUE(res);
      }
      {
         auto doc = arangodb::velocypack::Parser::fromJson(R"({"a":"2", "b":"2"})");
         arangodb::LocalDocumentId docId{3};
         // MSVC fails to compile if EXPECT_TRUE  is called directly
         auto res = _index->insert<arangodb::iresearch::InvertedIndexFieldIterator, 
                                   arangodb::iresearch::InvertedIndexFieldMeta>(
                                      trx, docId,
                                      doc->slice(), _index->_meta).ok();
         EXPECT_TRUE(res);
      }
      EXPECT_TRUE(trx.commitAsync().get().ok());
      EXPECT_TRUE(_index->commit(true).ok());
    }
    // second transaction to have more than one segment in the index
    {
      arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(vocbase()), EMPTY,
          collections, EMPTY, arangodb::transaction::Options());
      trx.begin();
      {
        auto doc = arangodb::velocypack::Parser::fromJson(R"({"a":"1", "b":"1"})");
        arangodb::LocalDocumentId docId{4};
        // MSVC fails to compile if EXPECT_TRUE  is called directly
        auto res =
            _index
                ->insert<arangodb::iresearch::InvertedIndexFieldIterator, arangodb::iresearch::InvertedIndexFieldMeta>(
                    trx, docId, doc->slice(), _index->_meta)
                .ok();
        EXPECT_TRUE(res);
      }
      {
        auto doc = arangodb::velocypack::Parser::fromJson(R"({"a":"3", "b":"3"})");
        arangodb::LocalDocumentId docId{5};
        // MSVC fails to compile if EXPECT_TRUE  is called directly
        auto res =
            _index
                ->insert<arangodb::iresearch::InvertedIndexFieldIterator, arangodb::iresearch::InvertedIndexFieldMeta>(
                    trx, docId, doc->slice(), _index->_meta)
                .ok();
        EXPECT_TRUE(res);
      }
      EXPECT_TRUE(trx.commitAsync().get().ok());
      EXPECT_TRUE(_index->commit(true).ok());
    }
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

  void executeIteratorTest(std::string_view queryString,
                           std::function<void(arangodb::IndexIterator* it)> const& test,
                           std::string_view refName = "d",
                           std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                           int mutableConditionIdx = -1) {
    SCOPED_TRACE(testing::Message("ExecuteIteratorTest failed for query ") << queryString); 
    auto ctx = std::make_shared<arangodb::transaction::StandaloneContext>(vocbase());
    auto query = arangodb::aql::Query::create(ctx,
                                              arangodb::aql::QueryString(queryString.data(),
                                                                         queryString.length()),
                                              bindVars);
    ASSERT_NE(query.get(), nullptr);
    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
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
    arangodb::IndexIteratorOptions opts;
    arangodb::SingleCollectionTransaction trx(
      ctx, collection(), arangodb::AccessMode::Type::READ);
    auto iterator = index().iteratorForCondition(
                    &collection(), &trx, filterNode, ref, opts, mutableConditionIdx);
    test(iterator.get());
  }


  arangodb::LogicalCollection& collection() {return *_collection;}
  TRI_vocbase_t& vocbase() { return *_vocbase; }
  arangodb::iresearch::IResearchInvertedIndex& index() { return *_index.get(); }
};  // IResearchFilterSetup

/// *IResearchInvertedIndexIteratorTest*:*IResearchInvertedIndex*:*LateMaterialization*:*NoMaterialization* --gtest_break_on_failure
TEST_F(IResearchInvertedIndexIteratorTest, test_skip) {
  std::string queryString{ R"(FOR d IN col FILTER d.a == "1" OR d.b == "2" RETURN d)"};
  auto test = [](arangodb::IndexIterator* iterator) {
    ASSERT_NE(iterator, nullptr);
    ASSERT_TRUE(iterator->hasMore());
    uint64_t skipped{0};
    iterator->skipAll(skipped);
    ASSERT_EQ(3, skipped);
    ASSERT_FALSE(iterator->hasMore());
  };
  executeIteratorTest(queryString, test);
}

TEST_F(IResearchInvertedIndexIteratorTest, test_skip_next) {
  std::string queryString{ R"(FOR d IN col FILTER d.a == "1" OR d.b == "2" RETURN d)"};
  auto test = [](arangodb::IndexIterator* iterator) {
    ASSERT_NE(iterator, nullptr);
    ASSERT_TRUE(iterator->hasMore());
    uint64_t skipped{0};
    iterator->skip(1, skipped);
    ASSERT_EQ(1, skipped);
    ASSERT_TRUE(iterator->hasMore());
    ASSERT_TRUE(iterator->hasCovering());
    std::vector<arangodb::LocalDocumentId> docs;
    auto docCallback = [&docs](arangodb::LocalDocumentId const& token) {
      docs.push_back(token);
      return true;
    };
    ASSERT_FALSE(iterator->next(docCallback, 1000));
    ASSERT_EQ(docs.size(), 2);
    // we can't assume order of the documents. Let's just check invalid ones are not returned
    ASSERT_EQ(std::find(docs.begin(), docs.end(), arangodb::LocalDocumentId(2)), docs.end());
    ASSERT_EQ(std::find(docs.begin(), docs.end(), arangodb::LocalDocumentId(5)), docs.end());
    ASSERT_FALSE(iterator->hasMore());
  };
  executeIteratorTest(queryString, test);
}
