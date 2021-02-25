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

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryOrTest : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryOrTest, test) {
  static std::vector<std::string> const EMPTY;

  auto createJson = VPackParser::fromJson(
      "{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;

  // add collection_1
  {
    auto collectionJson = VPackParser::fromJson(
        "{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection1);
  }

  // add collection_2
  {
    auto collectionJson = VPackParser::fromJson(
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
    auto updateJson = VPackParser::fromJson(
        "{ \"links\": {"
        "\"collection_1\": { \"analyzers\": [ \"test_analyzer\", \"identity\" "
        "], \"includeAllFields\": true, \"trackListPositions\": true, "
        "\"storeValues\":\"id\" },"
        "\"collection_2\": { \"analyzers\": [ \"test_analyzer\", \"identity\" "
        "], \"includeAllFields\": true, \"storeValues\":\"id\" }"
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
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // d.name == 'A' OR d.name == 'Q', d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      if (keySlice.isNone()) {
        continue;
      }
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key != "A" && key != "Q") {
        continue;
      }
      expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name == 'A' OR d.name == 'Q' SORT d.seq "
        "DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_EQUAL_SLICES(
          arangodb::velocypack::Slice(expectedDoc->second->vpack()),
          resolved);
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // d.name == 'X' OR d.same == 'xyz', BM25(d) DESC, TFIDF(d) DESC, d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name == 'X' OR d.same == 'xyz' SORT "
        "BM25(d) DESC, TFIDF(d) DESC, d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    // Check 1st (the most relevant doc)
    // {"name":"X","seq":23,"same":"xyz", "duplicated":"vczc", "prefix":"bateradsfsfasdf" }
    {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(expectedDocs[23]->vpack()),
                            resolved, true)));
      expectedDocs.erase(23);
    }

    // Check the rest of documents
    auto expectedDoc = expectedDocs.rbegin();
    for (resultIt.next(); resultIt.valid(); resultIt.next()) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                            resolved, true)));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // d.name == 'K' OR d.value <= 100 OR d.duplicated == abcd, TFIDF(d) DESC, d.seq DESC
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[10].vpack()),  // {"name":"K","seq":10,"same":"xyz","value":12,"duplicated":"abcd"}
        arangodb::velocypack::Slice(insertedDocs[30].vpack()),  // {"name":"$","seq":30,"same":"xyz","duplicated":"abcd","prefix":"abcy" }
        arangodb::velocypack::Slice(insertedDocs[26].vpack()),  // {"name":"~","seq":26,"same":"xyz", "duplicated":"abcd"}
        arangodb::velocypack::Slice(insertedDocs[20].vpack()),  // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
        arangodb::velocypack::Slice(insertedDocs[4].vpack()),  // {"name":"E","seq":4,"same":"xyz","value":100,"duplicated":"abcd"}
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz","value":100,"duplicated":"abcd","prefix":"abcd" }
        arangodb::velocypack::Slice(insertedDocs[16].vpack()),  // {"name":"Q","seq":16,"same":"xyz", "value":-32.5, "duplicated":"vczc"}
        arangodb::velocypack::Slice(insertedDocs[15].vpack()),  // {"name":"P","seq":15,"same":"xyz","value":50,"prefix":"abde"}
        arangodb::velocypack::Slice(insertedDocs[14].vpack()),  // {"name":"O","seq":14,"same":"xyz","value":0 }
        arangodb::velocypack::Slice(insertedDocs[13].vpack()),  // {"name":"N","seq":13,"same":"xyz","value":1,"duplicated":"vczc"}
        arangodb::velocypack::Slice(insertedDocs[12].vpack()),  // {"name":"M","seq":12,"same":"xyz","value":90.564 }
        arangodb::velocypack::Slice(insertedDocs[11].vpack()),  // {"name":"L","seq":11,"same":"xyz","value":95 }
        arangodb::velocypack::Slice(insertedDocs[9].vpack()),  // {"name":"J","seq":9,"same":"xyz","value":100 }
        arangodb::velocypack::Slice(insertedDocs[8].vpack()),  // {"name":"I","seq":8,"same":"xyz","value":100,"prefix":"bcd" }
        arangodb::velocypack::Slice(insertedDocs[6].vpack()),  // {"name":"G","seq":6,"same":"xyz","value":100 }
        arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz","value":12,"prefix":"abcde"}
    };

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name == 'K' OR d.value <= 100 OR "
        "d.duplicated == 'abcd' SORT TFIDF(d) DESC, d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    // Check the documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();
      resultIt.next();
      EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(*expectedDoc, resolved, true));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }

  // d.name == 'A' OR d.name == 'Q' OR d.same != 'xyz', d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      if (keySlice.isNone()) {
        continue;
      }
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key != "A" && key != "Q") {
        continue;
      }
      expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name == 'A' OR d.name == 'Q' OR d.same != "
        "'xyz' SORT d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_EQUAL_SLICES(arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                                        resolved);
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // d.name == 'F' OR EXISTS(d.duplicated), BM25(d) DESC, d.seq DESC
  {
    std::map<ptrdiff_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto const keySlice = docSlice.get("name");
      if (keySlice.isNone()) {
        continue;
      }
      auto const key = arangodb::iresearch::getStringRef(keySlice);
      if (key != "F" && docSlice.get("duplicated").isNone()) {
        continue;
      }
      expectedDocs.emplace(docSlice.get("seq").getNumber<ptrdiff_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name == 'F' OR EXISTS(d.duplicated) SORT "
        "BM25(d) DESC, d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    // Check 1st (the most relevant doc)
    // {"name":"F","seq":5,"same":"xyz", "value":1234 }
    {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(expectedDocs[5]->vpack()),
                            resolved, true)));
      expectedDocs.erase(5);
    }

    // Check the rest of documents
    auto expectedDoc = expectedDocs.rbegin();
    for (resultIt.next(); resultIt.valid(); resultIt.next()) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                            resolved, true)));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // d.name == 'D' OR STARTS_WITH(d.prefix, 'abc'), TFIDF(d) DESC, d.seq DESC
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs{
        // The most relevant document (satisfied both search conditions)
        arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}

        // Less relevant documents (satisfied STARTS_WITH condition only, has unqiue term in 'prefix' field)
        arangodb::velocypack::Slice(insertedDocs[25].vpack()),  // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" }
        arangodb::velocypack::Slice(insertedDocs[20].vpack()),  // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd", "prefix":"abcd" }

        // The least relevant documents (contain non-unique term 'abcy' in 'prefix' field)
        arangodb::velocypack::Slice(insertedDocs[31].vpack()),  // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
        arangodb::velocypack::Slice(insertedDocs[30].vpack()),  // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }
    };

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name == 'D' OR STARTS_WITH(d.prefix, "
        "'abc') SORT TFIDF(d) DESC, d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }

  // d.name == 'D' OR STARTS_WITH(d.prefix, 'abc'), BM25(d) DESC, d.seq DESC
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs{
        // The most relevant document (satisfied both search conditions)
        arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}

        // Less relevant documents (satisfied STARTS_WITH condition only, has unqiue term in 'prefix' field)
        arangodb::velocypack::Slice(insertedDocs[25].vpack()),  // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" }
        arangodb::velocypack::Slice(insertedDocs[20].vpack()),  // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd", "prefix":"abcd" }

        // The least relevant documents (contain non-unique term 'abcy' in 'prefix' field)
        arangodb::velocypack::Slice(insertedDocs[31].vpack()),  // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
        arangodb::velocypack::Slice(insertedDocs[30].vpack()),  // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }
    };

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name == 'D' OR STARTS_WITH(d.prefix, "
        "'abc') SORT BM25(d) DESC, d.seq DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }

  // d.name == 'D' OR STARTS_WITH(d.prefix, 'abc'), BM25(d) DESC, d.seq DESC, LIMIT 3
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs{
        // The most relevant document (satisfied both search conditions)
        arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}

        // Less relevant documents (satisfied STARTS_WITH condition only, has unqiue term in 'prefix' field)
        arangodb::velocypack::Slice(insertedDocs[25].vpack()),  // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" }
        arangodb::velocypack::Slice(insertedDocs[20].vpack()),  // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
    };

    auto queryResult = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.name == 'D' OR STARTS_WITH(d.prefix, "
        "'abc') SORT BM25(d) DESC, d.seq DESC LIMIT 3 RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
                            arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }

  // STARTS_WITH(d['prefix'], 'abc') OR EXISTS(d.duplicated) OR d.value < 100 OR d.name >= 'Z', BM25(d) DESC, TFIDF(d) DESC, d.seq DESC
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        arangodb::velocypack::Slice(insertedDocs[25].vpack()),  // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" ,
        arangodb::velocypack::Slice(insertedDocs[26].vpack()),  // {"name":"~","seq":26,"same":"xyz", "duplicated":"abcd"}

        arangodb::velocypack::Slice(insertedDocs[20].vpack()),  // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
        arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd", "prefix":"abcd" }
        arangodb::velocypack::Slice(insertedDocs[31].vpack()),  // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
        arangodb::velocypack::Slice(insertedDocs[30].vpack()),  // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }

        arangodb::velocypack::Slice(insertedDocs[23].vpack()),  // {"name":"X","seq":23,"same":"xyz", "duplicated":"vczc", "prefix":"bateradsfsfasdf" }
        arangodb::velocypack::Slice(insertedDocs[18].vpack()),  // {"name":"S","seq":18,"same":"xyz", "duplicated":"vczc"}
        arangodb::velocypack::Slice(insertedDocs[16].vpack()),  // {"name":"Q","seq":16,"same":"xyz", "value":-32.5, "duplicated":"vczc"}
        arangodb::velocypack::Slice(insertedDocs[15].vpack()),  // {"name":"P","seq":15,"same":"xyz","value":50, "prefix":"abde"},
        arangodb::velocypack::Slice(insertedDocs[14].vpack()),  // {"name":"O","seq":14,"same":"xyz","value":0 },
        arangodb::velocypack::Slice(insertedDocs[13].vpack()),  // {"name":"N","seq":13,"same":"xyz","value":1, "duplicated":"vczc"},
        arangodb::velocypack::Slice(insertedDocs[12].vpack()),  // {"name":"M","seq":12,"same":"xyz","value":90.564 },
        arangodb::velocypack::Slice(insertedDocs[11].vpack()),  // {"name":"L","seq":11,"same":"xyz","value":95 }
        arangodb::velocypack::Slice(insertedDocs[10].vpack()),  // {"name":"K","seq":10,"same":"xyz","value":12, "duplicated":"abcd"}
        arangodb::velocypack::Slice(insertedDocs[7].vpack()),  // {"name":"H","seq":7,"same":"xyz", "value":123, "duplicated":"vczc"},
        arangodb::velocypack::Slice(insertedDocs[4].vpack()),  // {"name":"E","seq":4,"same":"xyz", "value":100, "duplicated":"abcd"}
        arangodb::velocypack::Slice(insertedDocs[2].vpack()),  // {"name":"C","seq":2,"same":"xyz", "value":123, "duplicated":"vczc"}
        arangodb::velocypack::Slice(insertedDocs[1].vpack()),  // {"name":"B","seq":1,"same":"xyz", "value":101, "duplicated":"vczc"}
    };

    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH STARTS_WITH(d['prefix'], 'abc') OR "
        "EXISTS(d.duplicated) OR d.value < 100 OR d.name >= 'Z' SORT TFIDF(d) "
        "DESC, d.seq DESC RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }

    EXPECT_EQ(i, expected.size());
  }

  // PHRASE(d['duplicated'], 'v', 2, 'z', 'test_analyzer') OR STARTS_WITH(d['prefix'], 'abc') OR d.value > 100 OR d.name >= 'Z', BM25(d) DESC, TFIDF(d) DESC, d.seq DESC
  {
    std::vector<arangodb::velocypack::Slice> expected = {
        arangodb::velocypack::Slice(insertedDocs[25].vpack()),  // {"name":"Z","seq":25,"same":"xyz", "prefix":"abcdrer" ,
        arangodb::velocypack::Slice(insertedDocs[26].vpack()),  // {"name":"~","seq":26,"same":"xyz", "duplicated":"abcd"}
        arangodb::velocypack::Slice(insertedDocs[23].vpack()),  // {"name":"X","seq":23,"same":"xyz", "duplicated":"vczc", "prefix":"bateradsfsfasdf" }
        arangodb::velocypack::Slice(insertedDocs[18].vpack()),  // {"name":"S","seq":18,"same":"xyz", "duplicated":"vczc"}
        arangodb::velocypack::Slice(insertedDocs[16].vpack()),  // {"name":"Q","seq":16,"same":"xyz", "value":-32.5, "duplicated":"vczc"}
        arangodb::velocypack::Slice(insertedDocs[13].vpack()),  // {"name":"N","seq":13,"same":"xyz","value":1, "duplicated":"vczc"},
        arangodb::velocypack::Slice(insertedDocs[7].vpack()),  // {"name":"H","seq":7,"same":"xyz", "value":123, "duplicated":"vczc"},
        arangodb::velocypack::Slice(insertedDocs[2].vpack()),  // {"name":"C","seq":2,"same":"xyz", "value":123, "duplicated":"vczc"}
        arangodb::velocypack::Slice(insertedDocs[1].vpack()),  // {"name":"B","seq":1,"same":"xyz", "value":101, "duplicated":"vczc"}

        arangodb::velocypack::Slice(insertedDocs[20].vpack()),  // {"name":"U","seq":20,"same":"xyz", "prefix":"abc", "duplicated":"abcd"}
        arangodb::velocypack::Slice(insertedDocs[3].vpack()),  // {"name":"D","seq":3,"same":"xyz", "value":12, "prefix":"abcde"}
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),  // {"name":"A","seq":0,"same":"xyz", "value":100, "duplicated":"abcd", "prefix":"abcd" }
        arangodb::velocypack::Slice(insertedDocs[31].vpack()),  // {"name":"%","seq":31,"same":"xyz", "prefix":"abcy"}
        arangodb::velocypack::Slice(insertedDocs[30].vpack()),  // {"name":"$","seq":30,"same":"xyz", "duplicated":"abcd", "prefix":"abcy" }

        arangodb::velocypack::Slice(insertedDocs[15].vpack()),  // {"name":"P","seq":15,"same":"xyz","value":50, "prefix":"abde"},
        arangodb::velocypack::Slice(insertedDocs[14].vpack()),  // {"name":"O","seq":14,"same":"xyz","value":0 },
        arangodb::velocypack::Slice(insertedDocs[12].vpack()),  // {"name":"M","seq":12,"same":"xyz","value":90.564 },
        arangodb::velocypack::Slice(insertedDocs[11].vpack()),  // {"name":"L","seq":11,"same":"xyz","value":95 }
        arangodb::velocypack::Slice(insertedDocs[10].vpack()),  // {"name":"K","seq":10,"same":"xyz","value":12, "duplicated":"abcd"}
    };

    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'v', 1, 'z'), "
        "'test_analyzer') OR STARTS_WITH(d['prefix'], 'abc') OR d.value < 100 "
        "OR d.name >= 'Z' SORT TFIDF(d) DESC, d.seq DESC RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      EXPECT_TRUE(i < expected.size());

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(expected[i++],
                                                                    resolved, true)));
    }
    EXPECT_EQ(i, expected.size());
  }
}
