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

extern const char* ARGV0;  // defined in main.cpp

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryAggregateTest : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_F(IResearchQueryAggregateTest, test) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::vector<arangodb::velocypack::Builder> insertedDocs;
  arangodb::LogicalView* view;

  // create collection0
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

    std::vector<std::shared_ptr<arangodb::velocypack::Builder>> docs{
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -6, \"value\": null }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -5, \"value\": true }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -4, \"value\": \"abc\" }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -3, \"value\": 3.14 }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -2, \"value\": [ 1, \"abc\" ] }"),
        arangodb::velocypack::Parser::fromJson(
            "{ \"seq\": -1, \"value\": { \"a\": 7, \"b\": \"c\" } }"),
    };

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
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
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\" }");
    auto collection = vocbase.createCollection(createJson->slice());
    ASSERT_NE(nullptr, collection);

    irs::utf8_path resource;
    resource /= irs::string_ref(arangodb::tests::testResourceDir);
    resource /= irs::string_ref("simple_sequential.json");

    auto builder =
        arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
    auto slice = builder.slice();
    ASSERT_TRUE(slice.isArray());

    arangodb::OperationOptions options;
    options.returnNew = true;
    arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                              *collection,
                                              arangodb::AccessMode::Type::WRITE);
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
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_FALSE(!logicalView);

    view = logicalView.get();
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    ASSERT_FALSE(!impl);

    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": {"
        "\"testCollection0\": { \"includeAllFields\": true, "
        "\"trackListPositions\": true },"
        "\"testCollection1\": { \"includeAllFields\": true }"
        "}}");
    EXPECT_TRUE(impl->properties(updateJson->slice(), true).ok());
    std::set<arangodb::DataSourceId> cids;
    impl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_EQ(2, cids.size());
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // test grouping with counting
  {
    std::map<double_t, size_t> expected{
        {100., 5}, {12., 2}, {95., 1},   {90.564, 1}, {1., 1},
        {0., 1},   {50., 1}, {-32.5, 1}, {3.14, 1}  // null
    };

    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value <= 100 COLLECT value = d.value WITH "
        "COUNT INTO size RETURN { 'value' : value, 'names' : size }");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator itr(slice);
    ASSERT_EQ(expected.size(), itr.size());

    for (; itr.valid(); itr.next()) {
      auto const value = itr.value();
      auto const key = value.get("value").getNumber<double_t>();

      auto expectedValue = expected.find(key);
      ASSERT_NE(expectedValue, expected.end());
      ASSERT_EQ(expectedValue->second, value.get("names").getNumber<size_t>());
      ASSERT_EQ(1, expected.erase(key));
    }
    ASSERT_TRUE(expected.empty());
  }

  // test grouping
  {
    std::map<double_t, std::set<std::string>> expected{
        {100., {"A", "E", "G", "I", "J"}},
        {12., {"D", "K"}},
        {95., {"L"}},
        {90.564, {"M"}},
        {1., {"N"}},
        {0., {"O"}},
        {50., {"P"}},
        {-32.5, {"Q"}},
        {3.14, {}}  // null
    };

    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.value <= 100 COLLECT value = d.value INTO "
        "name = d.name RETURN { 'value' : value, 'names' : name }");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator itr(slice);
    ASSERT_EQ(expected.size(), itr.size());

    for (; itr.valid(); itr.next()) {
      auto const value = itr.value();
      auto const key = value.get("value").getNumber<double_t>();

      auto expectedValue = expected.find(key);
      ASSERT_NE(expectedValue, expected.end());

      arangodb::velocypack::ArrayIterator name(value.get("names"));
      auto& expectedNames = expectedValue->second;

      if (expectedNames.empty()) {
        // array must contain singe 'null' value
        ASSERT_EQ(1, name.size());
        ASSERT_TRUE(name.valid());
        ASSERT_TRUE(name.value().isNull());
        name.next();
        ASSERT_FALSE(name.valid());
      } else {
        ASSERT_EQ(expectedNames.size(), name.size());

        for (; name.valid(); name.next()) {
          auto const actualName = arangodb::iresearch::getStringRef(name.value());
          auto expectedName = expectedNames.find(actualName);
          ASSERT_NE(expectedName, expectedNames.end());
          expectedNames.erase(expectedName);
        }
      }

      ASSERT_TRUE(expectedNames.empty());
      ASSERT_EQ(1, expected.erase(key));
    }
    ASSERT_TRUE(expected.empty());
  }

  // test aggregation
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.seq < 7 COLLECT AGGREGATE sumSeq = "
        "SUM(d.seq) RETURN sumSeq");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator itr(slice);
    ASSERT_TRUE(itr.valid());
    EXPECT_EQ(0, itr.value().getNumber<size_t>());
    itr.next();
    EXPECT_FALSE(itr.valid());
  }

  // test aggregation without filter condition
  {
    auto result =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView COLLECT AGGREGATE "
                                      "sumSeq = SUM(d.seq) RETURN sumSeq");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator itr(slice);
    ASSERT_TRUE(itr.valid());
    EXPECT_EQ(475, itr.value().getNumber<size_t>());
    itr.next();
    EXPECT_FALSE(itr.valid());
  }

  // total number of documents in a view
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView COLLECT WITH COUNT INTO count RETURN count");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    arangodb::velocypack::ArrayIterator itr(slice);
    ASSERT_TRUE(itr.valid());
    EXPECT_EQ(38, itr.value().getNumber<size_t>());
    itr.next();
    EXPECT_FALSE(itr.valid());
  }
}
