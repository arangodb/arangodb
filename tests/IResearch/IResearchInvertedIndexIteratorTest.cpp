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
using DocsMap = std::map<arangodb::LocalDocumentId, std::shared_ptr<VPackBuilder>>;
using StoredFields = std::vector<std::vector<std::string>>;
using Fields = std::vector<std::string>;
using SortFields = std::vector<std::pair<std::string, bool>>;

constexpr std::string_view longStringValue {"longlonglonglonglonglonglonglonglonglonglonglonglong"};

class SimpleDataSetProvider {
 public:
  static DocsMap docs() {
    return {{arangodb::LocalDocumentId(1),
             arangodb::velocypack::Parser::fromJson(
                 R"({"a":"1", "b":"2", "c":"1longlonglonglonglonglonglonglonglonglonglonglonglong"})")},
            {arangodb::LocalDocumentId(2),
             arangodb::velocypack::Parser::fromJson(
                 R"({"a":"2", "b":"1", "c":"2longlonglonglonglonglonglonglonglonglonglonglonglong"})")},
            {arangodb::LocalDocumentId(3),
             arangodb::velocypack::Parser::fromJson(
                 R"({"a":"2", "b":"2", "c":"3longlonglonglonglonglonglonglonglonglonglonglonglong"})")},
            {arangodb::LocalDocumentId(4),
             arangodb::velocypack::Parser::fromJson(
                 R"({"a":"1", "b":"1", "c":"4longlonglonglonglonglonglonglonglonglonglonglonglong"})")},
            {arangodb::LocalDocumentId(5),
             arangodb::velocypack::Parser::fromJson(
                 R"({"a":"3", "b":"3", "c":"5longlonglonglonglonglonglonglonglonglonglonglonglong"})")}};
  }

  static StoredFields storedFields() { return {{"a", "b"}, {"a"}, {"b"}, {"c"}}; }

  static Fields indexFields() { return {"a", "b"}; }

  static SortFields sortFields() { return {}; }
};


class SortedSimpleDataSetProvider : public SimpleDataSetProvider {
 public:
  static SortFields sortFields() { return {{"a", true}, {"b", false}}; }
};

class ExtraDataSetProvider {
 public:
  static DocsMap docs() {
    return {{arangodb::LocalDocumentId(1),
             arangodb::velocypack::Parser::fromJson(R"({"_to":"2", "a":"1", "b":"2"})")},
            {arangodb::LocalDocumentId(2),
             arangodb::velocypack::Parser::fromJson(R"({"_from": "1", "_to":"2", "a":"2", "b":"1"})")},
            {arangodb::LocalDocumentId(3),
             arangodb::velocypack::Parser::fromJson(R"({"_from": "1", "_to":"2", "a":"2", "b":"2"})")},
            {arangodb::LocalDocumentId(4),
             arangodb::velocypack::Parser::fromJson(R"({"_from": "1", "_to":"2", "a":"1", "b":"1"})")},
            {arangodb::LocalDocumentId(5),
             arangodb::velocypack::Parser::fromJson(R"({"_from": "1", "a":"3", "b":"3"})")}};
  }

  static StoredFields storedFields() {
    return {{"_from"}, {"a", "b"}, {"a"}, {"b"}, {"_to"}};
  }

  static Fields indexFields() { return {"a", "b", "_from", "_to"}; }

  static SortFields sortFields() { return {}; }
};

} // namespace

template<typename Provider>
class IResearchInvertedIndexIteratorTestBase
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 private:
  DocsMap _docs;
  arangodb::tests::mocks::MockAqlServer _server{false};
  std::shared_ptr<arangodb::LogicalCollection> _analyzers;
  std::shared_ptr<arangodb::LogicalCollection> _collection;
  std::shared_ptr<arangodb::iresearch::IResearchInvertedIndex> _index;
  TRI_vocbase_t* _vocbase{nullptr};

 protected:
  IResearchInvertedIndexIteratorTestBase() {
    arangodb::tests::init();
    _docs = Provider::docs();
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
    auto storedFields = Provider::storedFields();
    auto sortedFields = Provider::sortFields();
    EXPECT_TRUE(meta.init(_server.server(),
                          getInvertedIndexPropertiesSlice(id, Provider::indexFields(), &storedFields, &sortedFields).slice(),
                          false, errorField, _vocbase->name()));
    _index = std::make_shared<arangodb::iresearch::IResearchInvertedIndex>(id, *_collection, std::move(meta));
    EXPECT_TRUE(_index);
    EXPECT_TRUE(_index->init().ok());

    // now populate the docs
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{_collection->name()};
    auto doc = _docs.begin();
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase()),
                                         EMPTY, collections, EMPTY,
                                         arangodb::transaction::Options());
      trx.begin();
      for(size_t i = 0; i < _docs.size() / 2; ++i){
         // MSVC fails to compile if EXPECT_TRUE  is called directly
         auto res = _index->insert<arangodb::iresearch::InvertedIndexFieldIterator, 
                                   arangodb::iresearch::InvertedIndexFieldMeta>(
                                      trx, doc->first,
                                      doc->second->slice(), _index->meta()).ok();
         EXPECT_TRUE(res);
         ++doc;
      }
      EXPECT_TRUE(trx.commitAsync().get().ok());
      EXPECT_TRUE(_index->commit(true).ok());
    }
    // second transaction to have more than one segment in the index
    arangodb::transaction::Methods trx(
       arangodb::transaction::StandaloneContext::Create(vocbase()), EMPTY,
       collections, EMPTY, arangodb::transaction::Options());
    trx.begin();
    while(doc != _docs.end()) {
      // MSVC fails to compile if EXPECT_TRUE  is called directly
      auto res = _index->insert<arangodb::iresearch::InvertedIndexFieldIterator,
                                arangodb::iresearch::InvertedIndexFieldMeta>(
                    trx, doc->first, doc->second->slice(), _index->meta()).ok();
      EXPECT_TRUE(res);
      ++doc;
    }
    EXPECT_TRUE(trx.commitAsync().get().ok());
    EXPECT_TRUE(_index->commit(true).ok());
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
                    &collection(), &trx, filterNode->getMember(0), ref, opts, mutableConditionIdx);
    test(iterator.get());
  }

  arangodb::LogicalCollection& collection() {return *_collection;}
  TRI_vocbase_t& vocbase() { return *_vocbase; }
  arangodb::iresearch::IResearchInvertedIndex& index() { return *_index.get(); }
  auto const& data() const { return _docs; }
};  // IResearchFilterSetup

using IResearchInvertedIndexIteratorTest = IResearchInvertedIndexIteratorTestBase<SimpleDataSetProvider>;

/// *IResearchInvertedIndexIteratorTest*:*IResearchInvertedIndex*:*LateMaterialization*:*NoMaterialization* --gtest_break_on_failure
TEST_F(IResearchInvertedIndexIteratorTest, test_skipAll) {
  std::string queryString{ R"(FOR d IN col FILTER d.a == "1" OR d.b == "2" RETURN d)"};
  auto test = [](arangodb::IndexIterator* iterator) {
    ASSERT_NE(iterator, nullptr);
    ASSERT_TRUE(iterator->hasMore());
    ASSERT_TRUE(iterator->hasCovering());
    ASSERT_FALSE(iterator->hasExtra());
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
    ASSERT_FALSE(iterator->hasExtra());
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

TEST_F(IResearchInvertedIndexIteratorTest, test_skip_next_skip) {
  std::string queryString{ R"(FOR d IN col FILTER d.a == "1" OR d.b == "2" RETURN d)"};
  auto test = [](arangodb::IndexIterator* iterator) {
    ASSERT_NE(iterator, nullptr);
    ASSERT_TRUE(iterator->hasMore());
    uint64_t skipped{0};
    iterator->skip(1, skipped);
    ASSERT_EQ(1, skipped);
    ASSERT_TRUE(iterator->hasMore());
    ASSERT_TRUE(iterator->hasCovering());
    ASSERT_FALSE(iterator->hasExtra());
    std::vector<arangodb::LocalDocumentId> docs;
    auto docCallback = [&docs](arangodb::LocalDocumentId const& token) {
      docs.push_back(token);
      return true;
    };
    ASSERT_TRUE(iterator->next(docCallback, 1));
    ASSERT_EQ(docs.size(), 1);
    // we can't assume order of the documents. Let's just check invalid ones are not returned
    ASSERT_EQ(std::find(docs.begin(), docs.end(), arangodb::LocalDocumentId(2)), docs.end());
    ASSERT_EQ(std::find(docs.begin(), docs.end(), arangodb::LocalDocumentId(5)), docs.end());
    ASSERT_TRUE(iterator->hasMore());
    uint64_t skipped2{0};
    iterator->skip(1000, skipped2);
    ASSERT_EQ(1, skipped2);
  };
  executeIteratorTest(queryString, test);
}

TEST_F(IResearchInvertedIndexIteratorTest, test_skip_nextCovering) {
  std::string queryString{ R"(FOR d IN col FILTER d.a == "1" OR d.b == "2" RETURN d)"};
  auto expectedDocs = data();
  auto test = [&expectedDocs](arangodb::IndexIterator* iterator) {
    ASSERT_NE(iterator, nullptr);
    ASSERT_TRUE(iterator->hasMore());
    uint64_t skipped{0};
    iterator->skip(1, skipped);
    ASSERT_EQ(1, skipped);
    ASSERT_TRUE(iterator->hasMore());
    ASSERT_TRUE(iterator->hasCovering());
    ASSERT_FALSE(iterator->hasExtra());
    std::vector<arangodb::LocalDocumentId> docs;
    
    auto docCallback = [&expectedDocs, &docs](arangodb::LocalDocumentId token,
                                              arangodb::IndexIterator::CoveringData* data) {
      docs.push_back(token);
      EXPECT_NE(data, nullptr);
      if (data) {
        EXPECT_TRUE(data->isArray());
        auto invalid = data->at(5);
        EXPECT_TRUE(invalid.isNone());
        {
          auto slice0  = data->at(0);
          EXPECT_TRUE(slice0.isString());
          if (slice0.isString()) {
            EXPECT_EQ(slice0.copyString(),
                      expectedDocs[token]->slice().get("a").copyString());
          }
          auto slice1  = data->at(1);
          EXPECT_TRUE(slice1.isString());
          if (slice1.isString()) {
            EXPECT_EQ(slice1.copyString(),
                      expectedDocs[token]->slice().get("b").copyString());
          }
        }
        {
          auto slice2  = data->at(2);
          EXPECT_TRUE(slice2.isString());
          if (slice2.isString()) {
            EXPECT_EQ(slice2.copyString(),
                      expectedDocs[token]->slice().get("a").copyString());
          }
        }
        {
          auto slice3  = data->at(3);
          EXPECT_TRUE(slice3.isString());
          if (slice3.isString()) {
            EXPECT_EQ(slice3.copyString(),
                      expectedDocs[token]->slice().get("b").copyString());
          }
        }
      }
      return true;
    };
    ASSERT_FALSE(iterator->nextCovering(docCallback, 1000));
    ASSERT_EQ(docs.size(), 2);
    // we can't assume order of the documents. Let's just check invalid ones are not returned
    ASSERT_EQ(std::find(docs.begin(), docs.end(), arangodb::LocalDocumentId(2)), docs.end());
    ASSERT_EQ(std::find(docs.begin(), docs.end(), arangodb::LocalDocumentId(5)), docs.end());
    ASSERT_FALSE(iterator->hasMore());
  };
  executeIteratorTest(queryString, test);
}

TEST_F(IResearchInvertedIndexIteratorTest, test_skip_nextCovering_skip) {
  std::string queryString{ R"(FOR d IN col FILTER d.a == "1" OR d.b == "2" OR d.b == "3" RETURN d)"};
  auto expectedDocs = data();
  auto test = [&expectedDocs](arangodb::IndexIterator* iterator) {
    ASSERT_NE(iterator, nullptr);
    ASSERT_TRUE(iterator->hasMore());
    uint64_t skipped{0};
    iterator->skip(1, skipped);
    ASSERT_EQ(1, skipped);
    ASSERT_TRUE(iterator->hasMore());
    ASSERT_TRUE(iterator->hasCovering());
    ASSERT_FALSE(iterator->hasExtra());
    std::vector<arangodb::LocalDocumentId> docs;
    
    auto docCallback = [&expectedDocs, &docs](arangodb::LocalDocumentId token,
                                              arangodb::IndexIterator::CoveringData* data) {
      docs.push_back(token);
      EXPECT_NE(data, nullptr);
      if (data) {
        EXPECT_TRUE(data->isArray());
        auto invalid = data->at(5);
        EXPECT_TRUE(invalid.isNone());
        {
          auto slice0  = data->at(0);
          EXPECT_TRUE(slice0.isString());
          if (slice0.isString()) {
            EXPECT_EQ(slice0.copyString(),
                      expectedDocs[token]->slice().get("a").copyString());
          }
          auto slice1  = data->at(1);
          EXPECT_TRUE(slice1.isString());
          if (slice1.isString()) {
            EXPECT_EQ(slice1.copyString(),
                      expectedDocs[token]->slice().get("b").copyString());
          }
        }
        {
          auto slice2  = data->at(2);
          EXPECT_TRUE(slice2.isString());
          if (slice2.isString()) {
            EXPECT_EQ(slice2.copyString(),
                      expectedDocs[token]->slice().get("a").copyString());
          }
        }
        {
          auto slice3  = data->at(3);
          EXPECT_TRUE(slice3.isString());
          if (slice3.isString()) {
            EXPECT_EQ(slice3.copyString(),
                      expectedDocs[token]->slice().get("b").copyString());
          }
        }
        {
          auto slice4 = data->at(4);
          EXPECT_TRUE(slice4.isString());
          if (slice4.isString()) {
            EXPECT_EQ(slice4.copyString(),
                      expectedDocs[token]->slice().get("c").copyString());
          }
        }
      }
      return true;
    };
    ASSERT_TRUE(iterator->nextCovering(docCallback, 1));
    ASSERT_EQ(docs.size(), 1);
    // we can't assume order of the documents. Let's just check invalid ones are not returned
    ASSERT_EQ(std::find(docs.begin(), docs.end(), arangodb::LocalDocumentId(2)), docs.end());
    ASSERT_TRUE(iterator->hasMore());
    docs.clear();
    ASSERT_TRUE(iterator->nextCovering(docCallback, 1));
    ASSERT_EQ(docs.size(), 1);
    // we can't assume order of the documents. Let's just check invalid ones are not returned
    ASSERT_EQ(std::find(docs.begin(), docs.end(), arangodb::LocalDocumentId(2)), docs.end());
    uint64_t skipped2{0};
    iterator->skipAll(skipped2);
    ASSERT_EQ(1, skipped2);
    ASSERT_FALSE(iterator->hasMore());
  };
  executeIteratorTest(queryString, test);
}

using IResearchInvertedIndexIteratorExtraTest = IResearchInvertedIndexIteratorTestBase<ExtraDataSetProvider>;

TEST_F(IResearchInvertedIndexIteratorExtraTest, test_skip_nextExtra_skip) {
  std::string queryString{ R"(FOR d IN col FILTER d._from == "1" AND (d.a == "1" OR d.b == "2" OR d.b == "3") RETURN d)"};
  auto expectedDocs = data();
  auto test = [&expectedDocs](arangodb::IndexIterator* iterator) {
    ASSERT_NE(iterator, nullptr);
    ASSERT_TRUE(iterator->hasMore());
    uint64_t skipped{0};
    iterator->skip(1, skipped);
    ASSERT_EQ(1, skipped);
    ASSERT_TRUE(iterator->hasMore());
    ASSERT_TRUE(iterator->hasCovering());
    ASSERT_TRUE(iterator->hasExtra());
    ASSERT_TRUE(iterator->canRearm());
    std::vector<arangodb::LocalDocumentId> docs;
    auto docCallback = [&expectedDocs, &docs](arangodb::LocalDocumentId token,
                                              VPackSlice extra) {
      docs.push_back(token);
      auto expectedSlice = expectedDocs[token]->slice().get("_to");
      EXPECT_EQUAL_SLICES(expectedSlice, extra);
      return true;
    };
    ASSERT_TRUE(iterator->nextExtra(docCallback, 1));
    ASSERT_EQ(docs.size(), 1);
    ASSERT_TRUE(iterator->hasMore());
    uint64_t skipped2{0};
    iterator->skipAll(skipped2);
    ASSERT_EQ(1, skipped2);
    ASSERT_FALSE(iterator->hasMore());
  };
  executeIteratorTest(queryString, test, "d", nullptr, 0);
}


using IResearchInvertedIndexIteratorSortedTest = IResearchInvertedIndexIteratorTestBase<SortedSimpleDataSetProvider>;


TEST_F(IResearchInvertedIndexIteratorSortedTest, test_next_full) {
  std::string queryString{ R"(FOR d IN col
                              FILTER (d.a == "1" OR d.b == "2" OR d.b == "3")
                              SORT d.a ASC, d.b DESC
                              RETURN d)"};
  std::vector<arangodb::LocalDocumentId> expectedDocs = {
      arangodb::LocalDocumentId(1), arangodb::LocalDocumentId(4),
      arangodb::LocalDocumentId(3), arangodb::LocalDocumentId(5)};
  auto test = [&expectedDocs](arangodb::IndexIterator* iterator) {
    ASSERT_NE(iterator, nullptr);
    ASSERT_TRUE(iterator->hasMore());
    ASSERT_TRUE(iterator->hasCovering());
    ASSERT_FALSE(iterator->hasExtra());
    ASSERT_FALSE(iterator->canRearm());
    std::vector<arangodb::LocalDocumentId> docs;
    auto docCallback = [&docs](arangodb::LocalDocumentId token) {
      docs.push_back(token);
      return true;
    };
    iterator->next(docCallback, 1000);
    ASSERT_EQ(docs.size(), expectedDocs.size());
    for (size_t i = 0; i < docs.size(); ++i) {
      ASSERT_EQ(docs[i], expectedDocs[i]);
    }
    ASSERT_FALSE(iterator->hasMore());
  };
  executeIteratorTest(queryString, test, "d", nullptr, -1);
}

TEST_F(IResearchInvertedIndexIteratorSortedTest, test_nextCovering_full) {
  std::string queryString{ R"(FOR d IN col
                              FILTER (d.a == "1" OR d.b == "2" OR d.b == "3")
                              SORT d.a ASC, d.b DESC
                              RETURN d)"};
  auto expectedDocs = data();
  auto test = [&expectedDocs](arangodb::IndexIterator* iterator) {
    std::vector<arangodb::LocalDocumentId> orderedDocs = {
        arangodb::LocalDocumentId(1), arangodb::LocalDocumentId(4),
        arangodb::LocalDocumentId(3), arangodb::LocalDocumentId(5)};
    ASSERT_NE(iterator, nullptr);
    ASSERT_TRUE(iterator->hasMore());
    ASSERT_TRUE(iterator->hasCovering());
    ASSERT_FALSE(iterator->hasExtra());
    ASSERT_FALSE(iterator->canRearm());
    std::vector<arangodb::LocalDocumentId> docs;
    auto docCallback = [&expectedDocs, &docs](arangodb::LocalDocumentId token,
                                              arangodb::IndexIterator::CoveringData* data) {
      docs.push_back(token);
      EXPECT_NE(data, nullptr);
      if (data) {
        EXPECT_TRUE(data->isArray());
        // sort columns counts also as stored so ivalid index is not 5 but 7
        auto invalid = data->at(7);
        EXPECT_TRUE(invalid.isNone());
        auto invalid2 = data->at(1000);
        EXPECT_TRUE(invalid2.isNone());
        {
          auto slice0  = data->at(0);
          EXPECT_TRUE(slice0.isString());
          if (slice0.isString()) {
            EXPECT_EQ(slice0.copyString(),
                      expectedDocs[token]->slice().get("a").copyString());
          }
          auto slice1  = data->at(1);
          EXPECT_TRUE(slice1.isString());
          if (slice1.isString()) {
            EXPECT_EQ(slice1.copyString(),
                      expectedDocs[token]->slice().get("b").copyString());
          }
        }
        {
          auto slice2 = data->at(2);
          EXPECT_TRUE(slice2.isString());
          if (slice2.isString()) {
            EXPECT_EQ(slice2.copyString(),
                      expectedDocs[token]->slice().get("a").copyString());
          }
          auto slice3 = data->at(3);
          EXPECT_TRUE(slice3.isString());
          if (slice3.isString()) {
            EXPECT_EQ(slice3.copyString(),
                      expectedDocs[token]->slice().get("b").copyString());
          }
        }
        {
          auto slice4  = data->at(4);
          EXPECT_TRUE(slice4.isString());
          if (slice4.isString()) {
            EXPECT_EQ(slice4.copyString(),
                      expectedDocs[token]->slice().get("a").copyString());
          }
        }
        {
          auto slice5  = data->at(5);
          EXPECT_TRUE(slice5.isString());
          if (slice5.isString()) {
            EXPECT_EQ(slice5.copyString(),
                      expectedDocs[token]->slice().get("b").copyString());
          }
        }
        {
          auto slice6 = data->at(6);
          EXPECT_TRUE(slice6.isString());
          if (slice6.isString()) {
            EXPECT_EQ(slice6.copyString(),
                      expectedDocs[token]->slice().get("c").copyString());
          }
        }
      }
      return true;
    };
    ASSERT_TRUE(iterator->nextCovering(docCallback, 2));
    ASSERT_TRUE(iterator->nextCovering(docCallback, 1));
    ASSERT_TRUE(iterator->nextCovering(docCallback, 1));
    ASSERT_TRUE(iterator->hasMore());
    ASSERT_FALSE(iterator->nextCovering(docCallback, 1));
    ASSERT_EQ(docs.size(), orderedDocs.size());
    for (size_t i = 0; i < docs.size(); ++i) {
      ASSERT_EQ(docs[i], orderedDocs[i]);
    }
    ASSERT_FALSE(iterator->hasMore());
  };
  executeIteratorTest(queryString, test, "d", nullptr, -1);
}
