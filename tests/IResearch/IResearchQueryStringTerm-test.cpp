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

#include "Aql/OptimizerRulesFeature.h"
#include "IResearch/IResearchView.h"
#include "Transaction/Helpers.h"
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

class IResearchQueryStringTermTest : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryStringTermTest, test) {
  // ArangoDB specific string comparer
  struct StringComparer {
    bool operator()(irs::string_ref const& lhs, irs::string_ref const& rhs) const {
      return arangodb::basics::VelocyPackHelper::compareStringValues(
                 lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size(), true) < 0;
    }
  };  // StringComparer

  // ==, !=, <, <=, >, >=, range
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
            collections[i % 2]->insert(&trx, doc, insertedDocs.back(), opt);
        EXPECT_TRUE(res.ok());
        ++i;
      }
    }

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_TRUE((arangodb::tests::executeQuery(
                     vocbase,
                     "FOR d IN testView SEARCH 1 ==1 OPTIONS { "
                     "waitForSync: true } RETURN d")
                     .result.ok()));  // commit
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                 system attributes
  // -----------------------------------------------------------------------------

  // _rev attribute
  {
    auto rev = arangodb::transaction::helpers::extractRevSliceFromDocument(
        VPackSlice(insertedDocs.front().vpack()));
    auto const revRef = arangodb::iresearch::getStringRef(rev);

    std::string const query = "FOR d IN testView SEARCH d._rev == '" +
                              static_cast<std::string>(revRef) + "' RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
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

  // _key attribute
  {
    auto key = arangodb::transaction::helpers::extractKeyPart(
        VPackSlice(insertedDocs.front().vpack()).get(arangodb::StaticStrings::KeyString));

    std::string const query =
        "FOR d IN testView SEARCH d._key == '" + key.toString() + "' RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
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

  // _id attribute
  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());

    auto const id = trx.extractIdString(VPackSlice(insertedDocs.front().vpack()));
    std::string const query =
        "FOR d IN testView SEARCH d._id == '" + id + "' RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
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

  // -----------------------------------------------------------------------------
  // --SECTION-- ==
  // -----------------------------------------------------------------------------

  // missing term
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'invalid_value' RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    std::string const query = "FOR d IN testView SEARCH d.name == 0 RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == null RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == false RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name == true RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name == @name RETURN d",
        arangodb::velocypack::Parser::fromJson("{ \"name\" : true }"));
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.name == 'A', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name == 'A' RETURN d");
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

  // d.same == 'same', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.same == 'xyz' RETURN d");
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

  // d.same == 'same', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.same == CONCAT('xy', @param) RETURN d",
        arangodb::velocypack::Parser::fromJson("{ \"param\" : \"z\" }"));
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

  // d.duplicated == 'abcd', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]},  {"E", &insertedDocs[4]},
        {"K", &insertedDocs[10]}, {"U", &insertedDocs[20]},
        {"~", &insertedDocs[26]}, {"$", &insertedDocs[30]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.duplicated == 'abcd' RETURN d");
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

  // d.duplicated == 'abcd', name DESC
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*, StringComparer> expectedDocs{
        {"A", &insertedDocs[0]},  {"E", &insertedDocs[4]},
        {"K", &insertedDocs[10]}, {"U", &insertedDocs[20]},
        {"~", &insertedDocs[26]}, {"$", &insertedDocs[30]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.duplicated == "
        "'abcd' SORT d.name DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator actualDocs(result);
    EXPECT_EQ(expectedDocs.size(), actualDocs.size());

    for (auto expectedDoc = expectedDocs.rbegin(), end = expectedDocs.rend();
         expectedDoc != end; ++expectedDoc) {
      EXPECT_TRUE(actualDocs.valid());
      auto actualDoc = actualDocs.value();
      auto const resolved = actualDoc.resolveExternals();
      std::string const actualName = resolved.get("name").toString();
      std::string const expectedName =
          arangodb::velocypack::Slice(expectedDoc->second->vpack()).get("name").toString();

      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      actualDocs.next();
    }
    EXPECT_FALSE(actualDocs.valid());
  }

  // d.duplicated == 'abcd', TFIDF() ASC, name DESC
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*, StringComparer> expectedDocs{
        {"A", &insertedDocs[0]},  {"E", &insertedDocs[4]},
        {"K", &insertedDocs[10]}, {"U", &insertedDocs[20]},
        {"~", &insertedDocs[26]}, {"$", &insertedDocs[30]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.duplicated == 'abcd' SORT TFIDF(d) ASC, "
        "d.name DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator actualDocs(result);
    EXPECT_EQ(expectedDocs.size(), actualDocs.size());

    for (auto expectedDoc = expectedDocs.rbegin(), end = expectedDocs.rend();
         expectedDoc != end; ++expectedDoc) {
      EXPECT_TRUE(actualDocs.valid());
      auto actualDoc = actualDocs.value();
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      actualDocs.next();
    }
    EXPECT_FALSE(actualDocs.valid());
  }

  // d.same == 'same', BM25() ASC, TFIDF() ASC, seq DESC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.same == 'xyz' SORT BM25(d) ASC, TFIDF(d) "
        "DESC, d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // expression  (invalid value)
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "LET x = RAND()"
        "LET z = {} "
        "FOR d IN testView SEARCH z.name == (x + (RAND() + 1)) RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // FIXME support expression with self-reference
  //  // expression  (invalid value)
  //  {
  //    auto queryResult = arangodb::tests::executeQuery(
  //      vocbase,
  //      "LET x = RAND()"
  //      "FOR d IN testView SEARCH d.name == (x + (RAND() + 1)) RETURN d"
  //    );
  //    ASSERT_TRUE(queryResult.result.ok());
  //
  //    auto result = queryResult.data->slice();
  //    EXPECT_TRUE(result.isArray());
  //
  //    arangodb::velocypack::ArrayIterator resultIt(result);
  //    EXPECT_EQ(0, resultIt.size());
  //
  //    for (auto const actualDoc : resultIt) {
  //      UNUSED(actualDoc);
  //      EXPECT_TRUE(false);
  //    }
  //  }

  // expression, d.duplicated == 'abcd', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]},  {"E", &insertedDocs[4]},
        {"K", &insertedDocs[10]}, {"U", &insertedDocs[20]},
        {"~", &insertedDocs[26]}, {"$", &insertedDocs[30]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "LET x = _NONDETERM_('abcd') "
        "FOR d IN testView SEARCH d.duplicated == x RETURN d");
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

  // expression+variable, d.duplicated == 'abcd', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]},  {"E", &insertedDocs[4]},
        {"K", &insertedDocs[10]}, {"U", &insertedDocs[20]},
        {"~", &insertedDocs[26]}, {"$", &insertedDocs[30]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "LET x = _NONDETERM_('abc') "
        "FOR d IN testView SEARCH d.duplicated == CONCAT(x, 'd') RETURN d");
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

  // expression+variable, d.duplicated == 'abcd', unordered, LIMIT 2
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}, {"E", &insertedDocs[4]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "LET x = _NONDETERM_('abc') "
        "FOR d IN testView SEARCH d.duplicated == "
        "CONCAT(x, 'd') LIMIT 2 RETURN d");
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

  // expression, d.duplicated == 'abcd', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]},  {"E", &insertedDocs[4]},
        {"K", &insertedDocs[10]}, {"U", &insertedDocs[20]},
        {"~", &insertedDocs[26]}, {"$", &insertedDocs[30]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.duplicated == "
        "CONCAT(_FORWARD_('abc'), 'd') RETURN d");
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

  // subquery, d.name == (FOR i IN collection_1 SEARCH i.name == 'A' RETURN i)[0].name), unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "LET x=(FOR i IN collection_1 FILTER "
                                      "i.name=='A' RETURN i)[0].name FOR d "
                                      "IN testView SEARCH d.name==x RETURN d");
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

  // subquery, d.name == (FOR i IN collection_1 SEARCH i.name == 'A' RETURN i)[0]), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "LET x=(FOR i IN collection_1 FILTER i.name=='A' RETURN i)[0] FOR d IN "
        "testView SEARCH d.name==x RETURN d");
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));  // unsupported type: object
  }

  // subquery, d.name == (FOR i IN collection_1 SEARCH i.name == 'A' RETURN i)[0].name), unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name==(FOR i IN collection_1 FILTER "
        "i.name=='A' RETURN i)[0].name RETURN d");
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

  // subquery, d.name == (FOR i IN collection_1 SEARCH i.name == 'A' RETURN i)[0]), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name==(FOR i IN collection_1 FILTER "
        "i.name=='A' RETURN i)[0] RETURN d");
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));  // unsupported type: object
  }

  // -----------------------------------------------------------------------------
  // --SECTION-- !=
  // -----------------------------------------------------------------------------

  // invalid type, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name != 0 RETURN d");
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

  // invalid type, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name != false RETURN d");
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

  // invalid type, d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name != null SORT d.seq DESC RETURN d");
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

  // missing term, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name != 'invalid_term' RETURN d");
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

  // existing duplicated term, unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.same != 'xyz' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // existing unique term, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    expectedDocs.erase("C");

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name != 'C' RETURN d");
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

  // existing unique term, unordered (not all documents contain field)
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;

    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const fieldSlice = docSlice.get("duplicated");

      if (!fieldSlice.isNone() && "vczc" == arangodb::iresearch::getStringRef(fieldSlice)) {
        continue;
      }

      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.duplicated != 'vczc' RETURN d");
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

  // missing term, seq DESC
  {
    auto& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name != "
        "'invalid_term' SORT d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // existing duplicated term, TFIDF() ASC, BM25() ASC, seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const fieldSlice = docSlice.get("duplicated");

      if (!fieldSlice.isNone() && "abcd" == arangodb::iresearch::getStringRef(fieldSlice)) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.duplicated != 'abcd' SORT TFIDF(d) ASC, "
        "BM25(d) ASC, d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(resultIt.size(), expectedDocs.size());

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

  // expression: invalid type, unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      expectedDocs.emplace(arangodb::iresearch::getStringRef(keySlice), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "LET x = _NONDETERM_(0) "
        "FOR d IN testView SEARCH d.name != x RETURN d");
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

  // expression: existing duplicated term, TFIDF() ASC, BM25() ASC, seq DESC LIMIT 10
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    size_t limit = 5;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const fieldSlice = docSlice.get("duplicated");

      if (!fieldSlice.isNone() && "abcd" == arangodb::iresearch::getStringRef(fieldSlice)) {
        continue;
      }

      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    // limit results
    while (expectedDocs.size() > limit) {
      expectedDocs.erase(expectedDocs.begin());
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "LET x = _NONDETERM_('abc') "
        "FOR d IN testView SEARCH d.duplicated != CONCAT(x,'d') SORT TFIDF(d) "
        "ASC, BM25(d) ASC, d.seq DESC LIMIT 5 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(resultIt.size(), expectedDocs.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      ++expectedDoc;

      if (!limit--) {
        break;
      }
    }

    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION-- <
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name < null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name < true RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name < 0 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.name < 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key >= "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name < 'H' RETURN d");
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

  // d.name < '!' (less than min term), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name < '!' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.name < '~' (less than max term), BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const nameSlice = docSlice.get("name");
      if (arangodb::iresearch::getStringRef(nameSlice) >= "~") {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name < '~' SORT BM25(d), TFIDF(d), d.seq "
        "DESC RETURN d");
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

  // -----------------------------------------------------------------------------
  // --SECTION-- <=
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name <= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name <= true RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name <= 0 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.name <= 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key > "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name <= 'H' RETURN d");
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

  // d.name <= '!' (less than min term), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name <= '!' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());

    auto const actualDoc = resultIt.value();
    auto const resolved = actualDoc.resolveExternals();
    EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                         arangodb::velocypack::Slice(insertedDocs[27].vpack()),
                         resolved, true));

    resultIt.next();
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name <= '~' (less than max term), BM25() ASC, TFIDF() ASC seq DESC
  {
    auto& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name <= '~' SORT BM25(d), TFIDF(d), d.seq "
        "DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION-- >
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name > null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name > true RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name > 0 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.name > 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key <= "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name > 'H' RETURN d");
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

  // d.name > '~' (greater than max term), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name > '~' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name > '!' (greater than min term), BM25() ASC, TFIDF() ASC seq DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const nameSlice = docSlice.get("name");
      if (arangodb::iresearch::getStringRef(nameSlice) <= "!") {
        continue;
      }
      auto const keySlice = docSlice.get("seq");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > '!' SORT BM25(d), TFIDF(d), d.seq "
        "DESC RETURN d");
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

  // -----------------------------------------------------------------------------
  // --SECTION-- >=
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name >= null RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name >= true RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name >= 0 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.name >= 'H', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key < "H") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name >= 'H' RETURN d");
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

  // d.name >= '~' (greater or equal than max term), unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name >= '~' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());

    auto const actualDoc = resultIt.value();
    auto const resolved = actualDoc.resolveExternals();
    EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                         arangodb::velocypack::Slice(insertedDocs[26].vpack()),
                         resolved, true));

    resultIt.next();
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name >= '!' (greater or equal than min term), BM25() ASC, TFIDF() ASC seq DESC
  {
    auto& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= '!' SORT BM25(d), TFIDF(d), d.seq "
        "DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                                      Range (>, <)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > null AND d.name < 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > true AND d.name < 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > 0 AND d.name < 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.name > 'H' AND d.name < 'S', unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key <= "H" || key >= "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > 'H' AND d.name < 'S' RETURN d");
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

  // d.name > 'S' AND d.name < 'N' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > 'S' AND d.name < 'N' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name > 'H' AND d.name < 'H' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > 'H' AND d.name < 'H' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name > '!' AND d.name < '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name <= "!" || name >= "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > '!' AND d.name < '~' SORT tfidf(d), "
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

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range (>=, <)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= null AND d.name < 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= true AND d.name < 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= 0 AND d.name < 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.name >= 'H' AND d.name < 'S' , unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key < "H" || key >= "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= 'H' AND d.name < 'S' RETURN d");
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

  // d.name >= 'S' AND d.name < 'N' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= 'S' AND d.name < 'N' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name >= 'H' AND d.name < 'H' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= 'H' AND d.name < 'H' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name >= '!' AND d.name < '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name < "!" || name >= "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH d.name >= '!' "
                                      "AND d.name < '~' SORT tfidf(d), "
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

  // -----------------------------------------------------------------------------
  // --SECTION--                                                     Range (>, <=)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > null AND d.name <= 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > true AND d.name <= 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > 0 AND d.name <= 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.name >= 'H' AND d.name <= 'S' , unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key <= "H" || key > "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > 'H' AND d.name <= 'S' RETURN d");
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

  // d.name > 'S' AND d.name <= 'N' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > 'S' AND d.name <= 'N' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name > 'H' AND d.name <= 'H' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name > 'H' AND d.name <= 'H' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name > '!' AND d.name <= '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name <= "!" || name > "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH d.name > '!' "
                                      "AND d.name <= '~' SORT tfidf(d), "
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

  // -----------------------------------------------------------------------------
  // --SECTION--                                                    Range (>=, <=)
  // -----------------------------------------------------------------------------

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= null AND d.name <= 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= true AND d.name <= 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // invalid type
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= 0 AND d.name <= 'Z' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());

    for (auto const actualDoc : resultIt) {
      UNUSED(actualDoc);
      EXPECT_TRUE(false);
    }
  }

  // d.name >= 'H' AND d.name <= 'S' , unordered
  {
    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key < "H" || key > "S") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= 'H' AND d.name <= 'S' RETURN d");
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

  // d.name >= 'S' AND d.name <= 'N' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= 'S' AND d.name <= 'N' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name >= 'H' AND d.name <= 'H' , unordered
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name >= 'H' AND d.name <= 'H' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());
    EXPECT_TRUE(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                         arangodb::velocypack::Slice(insertedDocs[7].vpack()), resolved, true));

    resultIt.next();
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name > '!' AND d.name <= '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("seq");
      auto const key = keySlice.getNumber<size_t>();
      auto const nameSlice = docSlice.get("name");
      auto const name = arangodb::iresearch::getStringRef(nameSlice);
      if (name < "!" || name > "~") {
        continue;
      }
      expectedDocs.emplace(key, &doc);
    }

    auto queryResult =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH d.name >= '!' "
                                      "AND d.name <= '~' SORT tfidf(d), "
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

  // -----------------------------------------------------------------------------
  // --SECTION--                                                    Range (>=, <=)
  // -----------------------------------------------------------------------------

  // d.name >= 'H' AND d.name <= 'S' , unordered
  // (will be converted to d.name >= 0 AND d.name <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name IN 'H'..'S' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // d.seq >= 'H' AND d.seq <= 'S' , unordered
  // (will be converted to d.seq >= 0 AND d.seq <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.seq IN 'H'..'S' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());
    EXPECT_TRUE(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                         arangodb::velocypack::Slice(insertedDocs[0].vpack()), resolved, true));

    resultIt.next();
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name >= 'S' AND d.name <= 'N' , unordered
  // (will be converted to d.name >= 0 AND d.name <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name IN 'S'..'N' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
  }

  // d.seq >= 'S' AND d.seq <= 'N' , unordered
  // (will be converted to d.seq >= 0 AND d.seq <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.seq IN 'S'..'N' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());
    EXPECT_TRUE(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                         arangodb::velocypack::Slice(insertedDocs[0].vpack()), resolved, true));

    resultIt.next();
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name >= 'H' AND d.name <= 'H' , unordered
  // (will be converted to d.name >= 0 AND d.name <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.name IN 'H'..'H' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
    ASSERT_TRUE(queryResult.result.ok());
  }

  // d.seq >= 'H' AND d.seq <= 'N' , unordered
  // (will be converted to d.seq >= 0 AND d.seq <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.seq IN 'H'..'N' RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(1, resultIt.size());
    EXPECT_TRUE(resultIt.valid());

    auto const resolved = resultIt.value().resolveExternals();
    EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                         arangodb::velocypack::Slice(insertedDocs[0].vpack()), resolved, true));

    resultIt.next();
    EXPECT_FALSE(resultIt.valid());
  }

  // d.name >= '!' AND d.name <= '~' , TFIDF() ASC, BM25() ASC, d.sec DESC
  // (will be converted to d.name >= 0 AND d.name <= 0)
  {
    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name IN '!'..'~' SORT tfidf(d), BM25(d), "
        "d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
    EXPECT_FALSE(resultIt.valid());
    ASSERT_TRUE(queryResult.result.ok());
  }
}
