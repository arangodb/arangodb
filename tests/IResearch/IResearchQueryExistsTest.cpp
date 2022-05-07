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

#include <velocypack/Iterator.h>

#include "utils/string_utils.hpp"

extern const char* ARGV0;  // defined in main.cpp

namespace {

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice systemDatabaseArgs = systemDatabaseBuilder.slice();

class IResearchQueryExistsTest : public IResearchQueryTest {};

}  // namespace

TEST_P(IResearchQueryExistsTest, test) {
  // The test has two variants: the original (variant == test"") and the one
  // from TEST_P(IResearchQueryExistsTest, StoreMaskPartially)
  // ("StoreMaskPartially"). Because the tests share a great part of the code,
  // they have been unified to improve compilation time.
  constexpr auto test = "test";
  constexpr auto storeMaskPartially = "StoreMaskPartially";
  for (auto variant : {test, storeMaskPartially}) {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    std::vector<arangodb::velocypack::Builder> insertedDocs;
    arangodb::LogicalView* view;

    // create collection0
    {
      auto createJson =
          VPackParser::fromJson("{ \"name\": \"testCollection0\" }");
      auto collection = vocbase.createCollection(createJson->slice());
      ASSERT_NE(nullptr, collection);

      std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
          VPackParser::fromJson("{ \"seq\": -6, \"value\": null }"),
          VPackParser::fromJson("{ \"seq\": -5, \"value\": true }"),
          VPackParser::fromJson("{ \"seq\": -4, \"value\": \"abc\" }"),
          VPackParser::fromJson("{ \"seq\": -3, \"value\": 3.14 }"),
          VPackParser::fromJson("{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
          VPackParser::fromJson(
              "{ \"seq\": -1, \"value\": { \"a\": 7, \"b\": \"c\" } }"),
      };

      arangodb::OperationOptions options;
      options.returnNew = true;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(vocbase),
          *collection, arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (auto& entry : docs) {
        auto res = trx.insert(collection->name(), entry->slice(), options);
        EXPECT_TRUE(res.ok());
        insertedDocs.emplace_back(res.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }

    // create collection1
    {
      auto createJson =
          VPackParser::fromJson("{ \"name\": \"testCollection1\" }");
      auto collection = vocbase.createCollection(createJson->slice());
      ASSERT_NE(nullptr, collection);

      irs::utf8_path resource;
      resource /= std::string_view(arangodb::tests::testResourceDir);
      resource /= std::string_view("simple_sequential.json");

      auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(
          resource.string());
      auto slice = builder.slice();
      ASSERT_TRUE(slice.isArray());

      arangodb::OperationOptions options;
      options.returnNew = true;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(vocbase),
          *collection, arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE(trx.begin().ok());

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto res = trx.insert(collection->name(), itr.value(), options);
        EXPECT_TRUE(res.ok());
        insertedDocs.emplace_back(res.slice().get("new"));
      }

      EXPECT_TRUE(trx.commit().ok());
    }

    // create view
    {
      auto createJson = VPackParser::fromJson(
          "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
      auto logicalView = vocbase.createView(createJson->slice());
      ASSERT_FALSE(!logicalView);

      view = logicalView.get();
      auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
      ASSERT_FALSE(!impl);

      auto viewDefinitionTemplate = variant == test ?
                                                    R"({ "links": {
      "testCollection0": {
        "includeAllFields": true,
        "version": %u,
        "trackListPositions": true,
        "storeValues": "id" },
      "testCollection1": {
        "includeAllFields": true,
        "version": %u,
        "storeValues": "id" }
    }})"
                                                    :
                                                    R"({ "links": {
      "testCollection0": {
        "includeAllFields": true,
        "trackListPositions": true,
        "version": %u },
      "testCollection1": {
        "includeAllFields": true,
        "storeValues": "id",
        "version": %u }
    }})";

      auto viewDefinition = irs::string_utils::to_string(
          viewDefinitionTemplate, static_cast<uint32_t>(linkVersion()),
          static_cast<uint32_t>(linkVersion()));

      auto updateJson = VPackParser::fromJson(viewDefinition);

      EXPECT_TRUE(impl->properties(updateJson->slice(), true, true).ok());
      std::set<arangodb::DataSourceId> cids;
      impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
        cids.emplace(cid);
        return true;
      });
      EXPECT_EQ(2, cids.size());
      EXPECT_TRUE(arangodb::tests::executeQuery(
                      vocbase,
                      "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                      "{ waitForSync: true } RETURN d")
                      .result.ok());  // commit
    }

    std::vector<arangodb::velocypack::Slice> const expectedEmpty = {};

    // test non-existent (any)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.missing) SORT BM25(d) ASC, "
          "TFIDF(d) "
          "DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (any) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['missing']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (bool)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.name, 'bool') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (bool) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['name'], 'bool') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (boolean)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.name, 'boolean') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (boolean) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['name'], 'boolean') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (numeric)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.name, 'numeric') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (numeric) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['name'], 'numeric') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (null)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.name, 'null') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (null) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['name'], 'null') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (string)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.seq, 'string') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (string) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['seq'], 'string') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (text analyzer)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.seq, 'analyzer', 'text_en') SORT "
          "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (text analyzer)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d.seq, 'analyzer'), "
          "'text_en') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (analyzer) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d['seq'], 'analyzer'), "
          "'text_en') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (array)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value[2]) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (array) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'][2]) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (object)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value.d) SORT BM25(d) ASC, "
          "TFIDF(d) "
          "DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    // test non-existent (object) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value']['d']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        EXPECT_TRUE(false);  // the loop should never be executed
      }

      EXPECT_EQ(i, expectedEmpty.size());
    }

    std::vector<arangodb::velocypack::Slice> const expected0_22 = {
        insertedDocs[0].slice(),  insertedDocs[1].slice(),
        insertedDocs[2].slice(),  insertedDocs[3].slice(),
        insertedDocs[4].slice(),  insertedDocs[5].slice(),
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(),
    };

    std::vector<arangodb::velocypack::Slice> const expected6_22 = {
        insertedDocs[6].slice(),  insertedDocs[7].slice(),
        insertedDocs[8].slice(),  insertedDocs[9].slice(),
        insertedDocs[10].slice(), insertedDocs[11].slice(),
        insertedDocs[12].slice(), insertedDocs[13].slice(),
        insertedDocs[14].slice(), insertedDocs[15].slice(),
        insertedDocs[16].slice(), insertedDocs[17].slice(),
        insertedDocs[18].slice(), insertedDocs[19].slice(),
        insertedDocs[20].slice(), insertedDocs[21].slice(),
        insertedDocs[22].slice(),
    };

    // test existent (any)
    {
      std::vector<arangodb::velocypack::Slice> const& expected =
          variant == test ? expected0_22 : expected6_22;
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value) SORT BM25(d) ASC, TFIDF(d) "
          "DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (any) via []
    {
      std::vector<arangodb::velocypack::Slice> const& expected =
          variant == test ? expected0_22 : expected6_22;
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    std::vector<arangodb::velocypack::Slice> const expected1 = {
        insertedDocs[1].slice(),
    };

    // test existent (bool)
    if (variant == test) {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'bool') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected1.size());
        EXPECT_EQUAL_SLICES(expected1[i++], resolved);
      }

      EXPECT_EQ(i, expected1.size());
    } else {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'bool') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      EXPECT_EQ(0, slice.length());
    }

    // test existent (bool) with bound params
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, @type) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          VPackParser::fromJson("{ \"type\" : \"bool\" }"));
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected1.size());
          EXPECT_EQUAL_SLICES(expected1[i++], resolved);
        }

        EXPECT_EQ(i, expected1.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (bool) with bound view name
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN  @@testView SEARCH EXISTS(d.value, @type) SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          VPackParser::fromJson(
              "{ \"type\" : \"bool\", \"@testView\": \"testView\" }"));

      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected1.size());
          EXPECT_EQUAL_SLICES(expected1[i++], resolved);
        }

        EXPECT_EQ(i, expected1.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (bool) with invalid bound view name
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN  @@testView SEARCH EXISTS(d.value, @type) SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d",
          VPackParser::fromJson(
              "{ \"type\" : \"bool\", \"@testView\": \"invlaidViewName\" }"));

      ASSERT_TRUE(result.result.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND));
    }

    // test existent (bool) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'bool') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected1.size());
          EXPECT_EQUAL_SLICES(expected1[i++], resolved);
        }

        EXPECT_EQ(i, expected1.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (boolean)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'boolean') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected1.size());
          EXPECT_EQUAL_SLICES(expected1[i++], resolved);
        }

        EXPECT_EQ(i, expected1.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (boolean) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'boolean') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected1.size());
          EXPECT_EQUAL_SLICES(expected1[i++], resolved);
        }

        EXPECT_EQ(i, expected1.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    std::vector<arangodb::velocypack::Slice> expected3_22 = {
        insertedDocs[3].slice(),  insertedDocs[6].slice(),
        insertedDocs[7].slice(),  insertedDocs[8].slice(),
        insertedDocs[9].slice(),  insertedDocs[10].slice(),
        insertedDocs[11].slice(), insertedDocs[12].slice(),
        insertedDocs[13].slice(), insertedDocs[14].slice(),
        insertedDocs[15].slice(), insertedDocs[16].slice(),
        insertedDocs[17].slice(), insertedDocs[18].slice(),
        insertedDocs[19].slice(), insertedDocs[20].slice(),
        insertedDocs[21].slice(), insertedDocs[22].slice(),
    };

    // test existent (numeric)
    {
      std::vector<arangodb::velocypack::Slice> const& expected =
          variant == test ? expected3_22 : expected6_22;
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'numeric') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    // test existent (numeric) via []
    {
      std::vector<arangodb::velocypack::Slice> const& expected =
          variant == test ? expected3_22 : expected6_22;
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'numeric') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    std::vector<arangodb::velocypack::Slice> const expected3_9 = {
        insertedDocs[3].slice(), insertedDocs[6].slice(),
        insertedDocs[7].slice(), insertedDocs[8].slice(),
        insertedDocs[9].slice()};
    std::vector<arangodb::velocypack::Slice> const expected6_10 = {
        insertedDocs[6].slice(), insertedDocs[7].slice(),
        insertedDocs[8].slice(), insertedDocs[9].slice(),
        insertedDocs[10].slice()};

    // test existent (numeric) via [], limit  5
    {
      std::vector<arangodb::velocypack::Slice> expected =
          variant == test ? expected3_9 : expected6_10;
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'numeric') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq LIMIT 5 RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      size_t i = 0;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto const resolved = itr.value().resolveExternals();
        EXPECT_TRUE(i < expected.size());
        EXPECT_EQUAL_SLICES(expected[i++], resolved);
      }

      EXPECT_EQ(i, expected.size());
    }

    std::vector<arangodb::velocypack::Slice> const expected0 = {
        insertedDocs[0].slice(),
    };

    // test existent (null)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'null') SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected0.size());
          EXPECT_EQUAL_SLICES(expected0[i++], resolved);
        }

        EXPECT_EQ(i, expected0.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (null) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'null') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected0.size());
          EXPECT_EQUAL_SLICES(expected0[i++], resolved);
        }

        EXPECT_EQ(i, expected0.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    std::vector<arangodb::velocypack::Slice> expected2 = {
        insertedDocs[2].slice(),
    };

    // test existent (identity analyzer)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'analyzer') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected2.size());
          EXPECT_EQUAL_SLICES(expected2[i++], resolved);
        }

        EXPECT_EQ(i, expected2.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (identity analyzer)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'analyzer', 'identity') "
          "SORT "
          "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected2.size());
          EXPECT_EQUAL_SLICES(expected2[i++], resolved);
        }

        EXPECT_EQ(i, expected2.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (string)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d.value, 'analyzer'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected2.size());
          EXPECT_EQUAL_SLICES(expected2[i++], resolved);
        }

        EXPECT_EQ(i, expected2.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (any string)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d.value, 'string'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected2.size());
          EXPECT_EQUAL_SLICES(expected2[i++], resolved);
        }

        EXPECT_EQ(i, expected2.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (any string)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'string') SORT BM25(d) "
          "ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected2.size());
          EXPECT_EQUAL_SLICES(expected2[i++], resolved);
        }

        EXPECT_EQ(i, expected2.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (any string) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'string') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected2.size());
          EXPECT_EQUAL_SLICES(expected2[i++], resolved);
        }

        EXPECT_EQ(i, expected2.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (identity analyzer)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'analyzer', 'identity') "
          "SORT "
          "BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected2.size());
          EXPECT_EQUAL_SLICES(expected2[i++], resolved);
        }

        EXPECT_EQ(i, expected2.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (identity analyzer)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value, 'analyzer') SORT BM25(d) "
          "ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected2.size());
          EXPECT_EQUAL_SLICES(expected2[i++], resolved);
        }

        EXPECT_EQ(i, expected2.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (identity analyzer) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d['value'], 'analyzer'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected2.size());
          EXPECT_EQUAL_SLICES(expected2[i++], resolved);
        }

        EXPECT_EQ(i, expected2.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (identity analyzer) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH ANALYZER(EXISTS(d['value'], 'analyzer'), "
          "'identity') SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected2.size());
          EXPECT_EQUAL_SLICES(expected2[i++], resolved);
        }

        EXPECT_EQ(i, expected2.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (identity analyzer) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'], 'analyzer', 'identity') "
          "SORT BM25(d) ASC, TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected2.size());
          EXPECT_EQUAL_SLICES(expected2[i++], resolved);
        }

        EXPECT_EQ(i, expected2.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    std::vector<arangodb::velocypack::Slice> const expected4 = {
        insertedDocs[4].slice(),
    };

    // test existent (array)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value[1]) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected4.size());
          EXPECT_EQUAL_SLICES(expected4[i++], resolved);
        }

        EXPECT_EQ(i, expected4.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (array) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value'][1]) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected4.size());
          EXPECT_EQUAL_SLICES(expected4[i++], resolved);
        }

        EXPECT_EQ(i, expected4.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    std::vector<arangodb::velocypack::Slice> const expected5 = {
        insertedDocs[5].slice(),
    };

    // test existent (object)
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d.value.b) SORT BM25(d) ASC, "
          "TFIDF(d) "
          "DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected5.size());
          EXPECT_EQUAL_SLICES(expected5[i++], resolved);
        }

        EXPECT_EQ(i, expected5.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }

    // test existent (object) via []
    {
      auto result = arangodb::tests::executeQuery(
          vocbase,
          "FOR d IN testView SEARCH EXISTS(d['value']['b']) SORT BM25(d) ASC, "
          "TFIDF(d) DESC, d.seq RETURN d");
      ASSERT_TRUE(result.result.ok());
      auto slice = result.data->slice();
      EXPECT_TRUE(slice.isArray());
      if (variant == test) {
        size_t i = 0;

        for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid();
             ++itr) {
          auto const resolved = itr.value().resolveExternals();
          EXPECT_TRUE(i < expected5.size());
          EXPECT_EQUAL_SLICES(expected5[i++], resolved);
        }

        EXPECT_EQ(i, expected5.size());
      } else {
        EXPECT_EQ(0, slice.length());
      }
    }
  }
}

INSTANTIATE_TEST_CASE_P(IResearchQueryExistsTest, IResearchQueryExistsTest,
                        GetLinkVersions());
