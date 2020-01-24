////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchQueryCommon.h"

#include "IResearch/IResearchView.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>

extern const char* ARGV0;  // defined in main.cpp

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryStartsWithTest : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryStartsWithTest, test) {
  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;

  // add collection_1
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection1);
  }

  // add collection_2
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"collection_2\" }");
    logicalCollection2 = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection2);
  }

  // add view
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(createJson->slice()));
  ASSERT_FALSE(!view);

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\" : {"
        "\"collection_1\" : { \"includeAllFields\" : true },"
        "\"collection_2\" : { \"includeAllFields\" : true }"
        "}}");
    EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
    builder.close();

    auto slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_EQ(slice.get("name").copyString(), "testView");
    EXPECT_TRUE(slice.get("type").copyString() ==
                arangodb::iresearch::DATA_SOURCE_TYPE.name());
    EXPECT_TRUE(slice.get("deleted").isNone());  // no system properties
    auto tmpSlice = slice.get("links");
    EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
  }

  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  // populate view with the data
  {
    arangodb::OperationOptions opt;

    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());

    // insert into collections
    {
      irs::utf8_path resource;
      resource /= irs::string_ref(arangodb::tests::testResourceDir);
      resource /= irs::string_ref("simple_sequential.json");

      auto builder =
          arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      ASSERT_TRUE(root.isArray());

      size_t i = 0;

      std::shared_ptr<arangodb::LogicalCollection> collections[]{logicalCollection1,
                                                                 logicalCollection2};

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocs.emplace_back();
        auto const res =
            collections[i % 2]->insert(&trx, doc, insertedDocs.back(), opt, false);
        EXPECT_TRUE(res.ok());
        ++i;
      }
    }

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // invalid field
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH STARTS_WITH(d.invalid_field, 'abc') RETURN "
        "d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH STARTS_WITH(d.seq, '0') RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // execution outside arangosearch (true)
  {
    auto queryResult = arangodb::tests::executeQuery(vocbase, "RETURN starts_with('abc', 'a')");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      ASSERT_TRUE(resolved.isBool());
      ASSERT_TRUE(resolved.getBool());
    }
  }

  // execution outside arangosearch (true)
  {
    auto queryResult = arangodb::tests::executeQuery(vocbase, "RETURN starts_with('abc', 'abc')");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      ASSERT_TRUE(resolved.isBool());
      ASSERT_TRUE(resolved.getBool());
    }
  }

  // execution outside arangosearch (false)
  {
    auto queryResult = arangodb::tests::executeQuery(vocbase, "RETURN starts_with('a', 'abc')");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      ASSERT_TRUE(resolved.isBool());
      ASSERT_FALSE(resolved.getBool());
    }
  }

  // execution outside arangosearch (wrong args)
  {
    auto queryResult = arangodb::tests::executeQuery(vocbase, "RETURN starts_with(1, 'abc')");
    ASSERT_TRUE(queryResult.result.ok());
    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());
    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      ASSERT_TRUE(resolved.isNull());
    }
  }

  // execution outside arangosearch (wrong args)
  {
    auto queryResult = arangodb::tests::executeQuery(vocbase, "RETURN starts_with(true, 'abc')");
    ASSERT_TRUE(queryResult.result.ok());
    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());
    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      ASSERT_TRUE(resolved.isNull());
    }
  }

  // execution outside arangosearch (wrong args)
  {
    auto queryResult = arangodb::tests::executeQuery(vocbase, "RETURN starts_with(null, 'abc')");
    ASSERT_TRUE(queryResult.result.ok());
    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());
    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      ASSERT_TRUE(resolved.isNull());
    }
  }

  // execution outside arangosearch (wrong args)
  {
    auto queryResult = arangodb::tests::executeQuery(vocbase, "RETURN starts_with('a', 1)");
    ASSERT_TRUE(queryResult.result.ok());
    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());
    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      ASSERT_TRUE(resolved.isNull());
    }
  }

  // execution outside arangosearch (wrong args)
  {
    auto queryResult = arangodb::tests::executeQuery(vocbase, "RETURN starts_with('a', null)");
    ASSERT_TRUE(queryResult.result.ok());
    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());
    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      ASSERT_TRUE(resolved.isNull());
    }
  }

  // execution outside arangosearch (wrong args)
  {
    auto queryResult = arangodb::tests::executeQuery(vocbase, "RETURN starts_with('a', true)");
    ASSERT_TRUE(queryResult.result.ok());
    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());
    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      ASSERT_TRUE(resolved.isNull());
    }
  }

  // exact term, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH starts_with(d.name, 'A') RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_NE(expectedDoc, expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // exact term, ordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH starts_with(d.name, 'A', 0) SORT TFIDF(d) "
        "DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_NE(expectedDoc, expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // d.prefix = abc*, d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const prefixSlice = docSlice.get("prefix");
      if (prefixSlice.isNone() ||
          !irs::starts_with(arangodb::iresearch::getStringRef(prefixSlice),
                            "abc")) {
        continue;
      }
      expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH starts_with(d.prefix, 'abc') SORT d.seq DESC "
        "RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // Empty prefix - return all docs: d.prefix = ''*, TFIDF(), BM25(), d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const prefixSlice = docSlice.get("prefix");
      if (prefixSlice.isNone()) {
        continue;
      }
      expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH starts_with(d.prefix, '') SORT TFIDF(d), "
        "BM25(d), d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // invalid prefix
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH STARTS_WITH(d.prefix, 'abc_invalid_prefix') "
        "RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }
}
