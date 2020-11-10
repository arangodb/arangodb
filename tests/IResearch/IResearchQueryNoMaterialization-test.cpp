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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchQueryCommon.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/OptimizerRulesFeature.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewStoredValues.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>

namespace {

static const char* collectionName1 = "collection_1";
static const char* collectionName2 = "collection_2";

static const char* viewName = "view";
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryNoMaterializationTest : public IResearchQueryTest {
 protected:
  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  void addLinkToCollection(std::shared_ptr<arangodb::iresearch::IResearchView>& view) {
    auto updateJson = VPackParser::fromJson(
      std::string("{") +
        "\"links\": {"
        "\"" + collectionName1 + "\": {\"includeAllFields\": true, \"storeValues\": \"id\"},"
        "\"" + collectionName2 + "\": {\"includeAllFields\": true, \"storeValues\": \"id\"}"
      "}}");
    EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
    builder.close();

    auto slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(slice.get("type").copyString() ==
                arangodb::iresearch::DATA_SOURCE_TYPE.name());
    EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
    auto tmpSlice = slice.get("links");
    EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
  }

  void SetUp() override {
    // add collection_1
    std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
    {
      auto collectionJson =
          VPackParser::fromJson(std::string("{\"name\": \"") + collectionName1 + "\"}");
      logicalCollection1 = vocbase().createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection1);
    }

    // add collection_2
    std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
    {
      auto collectionJson =
          VPackParser::fromJson(std::string("{\"name\": \"") + collectionName2 + "\"}");
      logicalCollection2 = vocbase().createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection2);
    }
    // create view
    std::shared_ptr<arangodb::iresearch::IResearchView> view;
    {
      auto createJson = VPackParser::fromJson(
          std::string("{") +
          "\"name\": \"" + viewName + "\", \
           \"type\": \"arangosearch\", \
           \"primarySort\": [{\"field\": \"value\", \"direction\": \"asc\"}, {\"field\": \"foo\", \"direction\": \"desc\"}], \
           \"storedValues\": [{\"fields\":[\"str\"], \"compression\":\"none\"}, [\"value\"], [\"_id\"], [\"str\", \"value\"], [\"exist\"]] \
        }");
      view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
          vocbase().createView(createJson->slice()));
      ASSERT_FALSE(!view);

      // add links to collections
      addLinkToCollection(view);
    }

    // populate view with the data
    {
      arangodb::OperationOptions opt;
      static std::vector<std::string> const EMPTY;
      arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(vocbase()),
          EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collection_1
      {
        auto builder = VPackParser::fromJson(
            "["
              "{\"_key\": \"c0\", \"str\": \"cat\", \"foo\": \"foo0\", \"value\": 0, \"exist\": \"ex0\"},"
              "{\"_key\": \"c1\", \"str\": \"cat\", \"foo\": \"foo1\", \"value\": 1},"
              "{\"_key\": \"c2\", \"str\": \"cat\", \"foo\": \"foo2\", \"value\": 2, \"exist\": \"ex2\"},"
              "{\"_key\": \"c3\", \"str\": \"cat\", \"foo\": \"foo3\", \"value\": 3}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          insertedDocs.emplace_back();
          auto const res =
              logicalCollection1->insert(&trx, doc, insertedDocs.back(), opt);
          EXPECT_TRUE(res.ok());
        }
      }

      // insert into collection_2
      {
        auto builder = VPackParser::fromJson(
            "["
              "{\"_key\": \"c_0\", \"str\": \"cat\", \"foo\": \"foo_0\", \"value\": 10, \"exist\": \"ex_10\"},"
              "{\"_key\": \"c_1\", \"str\": \"cat\", \"foo\": \"foo_1\", \"value\": 11},"
              "{\"_key\": \"c_2\", \"str\": \"cat\", \"foo\": \"foo_2\", \"value\": 12, \"exist\": \"ex_12\"},"
              "{\"_key\": \"c_3\", \"str\": \"cat\", \"foo\": \"foo_3\", \"value\": 13}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          insertedDocs.emplace_back();
          auto const res =
              logicalCollection2->insert(&trx, doc, insertedDocs.back(), opt);
          EXPECT_TRUE(res.ok());
        }
      }

      EXPECT_TRUE(trx.commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection1, *view)
                  ->commit().ok());

      EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection2, *view)
                  ->commit().ok());
    }
  }

  void executeAndCheck(std::string const& queryString,
                       std::vector<VPackValue> const& expectedValues,
                       arangodb::velocypack::ValueLength numOfColumns,
                       std::set<std::pair<int, size_t>>&& fields) {
    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), queryString,
      {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase()),
                               arangodb::aql::QueryString(queryString), nullptr,
                               arangodb::velocypack::Parser::fromJson("{}"));
    auto const res = query.explain();
    ASSERT_TRUE(res.data);
    auto const explanation = res.data->slice();
    arangodb::velocypack::ArrayIterator nodes(explanation.get("nodes"));
    auto found = false;
    for (auto const node : nodes) {
      if (node.hasKey("type") && node.get("type").isString() && node.get("type").stringRef() == "EnumerateViewNode") {
        EXPECT_TRUE(node.hasKey("noMaterialization") && node.get("noMaterialization").isBool() && node.get("noMaterialization").getBool());
        ASSERT_TRUE(node.hasKey("viewValuesVars") && node.get("viewValuesVars").isArray());
        ASSERT_EQ(numOfColumns, node.get("viewValuesVars").length());
        arangodb::velocypack::ArrayIterator columnFields(node.get("viewValuesVars"));
        for (auto const cf : columnFields) {
          ASSERT_TRUE(cf.isObject());
          if (cf.hasKey("fieldNumber")) {
            auto fieldNumber = cf.get("fieldNumber");
            ASSERT_TRUE(fieldNumber.isNumber<size_t>());
            auto it = fields.find(std::make_pair(arangodb::iresearch::IResearchViewNode::SortColumnNumber, fieldNumber.getNumber<size_t>()));
            ASSERT_TRUE(it != fields.end());
            fields.erase(it);
          } else {
            ASSERT_TRUE(cf.hasKey("columnNumber") && cf.get("columnNumber").isNumber());
            auto columnNumber = cf.get("columnNumber").getNumber<int>();
            ASSERT_TRUE(cf.hasKey("viewStoredValuesVars") && cf.get("viewStoredValuesVars").isArray());
            arangodb::velocypack::ArrayIterator fs(cf.get("viewStoredValuesVars"));
            for (auto const f : fs) {
              ASSERT_TRUE(f.hasKey("fieldNumber") && f.get("fieldNumber").isNumber<size_t>());
              auto it = fields.find(std::make_pair(columnNumber, f.get("fieldNumber").getNumber<size_t>()));
              ASSERT_TRUE(it != fields.end());
              fields.erase(it);
            }
          }
        }
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(fields.empty());

    auto queryResult = arangodb::tests::executeQuery(vocbase(), queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);

    ASSERT_EQ(expectedValues.size(), resultIt.size());
    // Check values
    auto expectedValue = expectedValues.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedValue) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      if (resolved.isString()) {
        ASSERT_TRUE(expectedValue->isString());
        arangodb::velocypack::ValueLength length = 0;
        auto resStr = resolved.getString(length);
        EXPECT_TRUE(memcmp(expectedValue->getCharPtr(), resStr, length) == 0);
      } else {
        ASSERT_TRUE(resolved.isNumber());
        EXPECT_EQ(expectedValue->getInt64(), resolved.getInt());
      }
    }
    EXPECT_EQ(expectedValue, expectedValues.end());
  }
};

}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryNoMaterializationTest, sortColumnPriority) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " SEARCH d.value IN [1, 2, 11, 12] SORT d.value RETURN d.value";

  std::vector<VPackValue> expectedValues{
    VPackValue(1),
    VPackValue(2),
    VPackValue(11),
    VPackValue(12)
  };

  executeAndCheck(queryString, expectedValues, 1, {{arangodb::iresearch::IResearchViewNode::SortColumnNumber, 0}});
}

TEST_F(IResearchQueryNoMaterializationTest, maxMatchColumnPriority) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " FILTER d.str == 'cat' SORT d.value RETURN d.value";

  std::vector<VPackValue> expectedValues{
    VPackValue(0),
    VPackValue(1),
    VPackValue(2),
    VPackValue(3),
    VPackValue(10),
    VPackValue(11),
    VPackValue(12),
    VPackValue(13)
  };

  executeAndCheck(queryString, expectedValues, 1, {{3, 0}, {3, 1}});
}

TEST_F(IResearchQueryNoMaterializationTest, sortAndStoredValues) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " SORT d._id RETURN d.foo";

  std::vector<VPackValue> expectedValues{
    VPackValue("foo0"),
    VPackValue("foo1"),
    VPackValue("foo2"),
    VPackValue("foo3"),
    VPackValue("foo_0"),
    VPackValue("foo_1"),
    VPackValue("foo_2"),
    VPackValue("foo_3")
  };

  executeAndCheck(queryString, expectedValues, 2, {{arangodb::iresearch::IResearchViewNode::SortColumnNumber, 1}, {2, 0}});
}

TEST_F(IResearchQueryNoMaterializationTest, fieldExistence) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " SEARCH EXISTS(d.exist) SORT d.value RETURN d.value";

  std::vector<VPackValue> expectedValues{
    VPackValue(0),
    VPackValue(2),
    VPackValue(10),
    VPackValue(12)
  };

  executeAndCheck(queryString, expectedValues, 1, {{arangodb::iresearch::IResearchViewNode::SortColumnNumber, 0}});
}

TEST_F(IResearchQueryNoMaterializationTest, storedFieldExistence) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " SEARCH EXISTS(d.exist) SORT d.value RETURN d.exist";

  std::vector<VPackValue> expectedValues{
    VPackValue("ex0"),
    VPackValue("ex2"),
    VPackValue("ex_10"),
    VPackValue("ex_12")
  };

  executeAndCheck(queryString, expectedValues, 2, {{arangodb::iresearch::IResearchViewNode::SortColumnNumber, 0}, {4, 0}});
}

TEST_F(IResearchQueryNoMaterializationTest, emptyField) {
  auto const queryString = std::string("FOR d IN ") + viewName +
      " SORT d.exist DESC LIMIT 1 RETURN d.exist";

  std::vector<VPackValue> expectedValues{
    VPackValue("ex2")
  };

  executeAndCheck(queryString, expectedValues, 1, {{4, 0}});
}

TEST_F(IResearchQueryNoMaterializationTest, testStoredValuesRecord) {
  static std::vector<std::string> const EMPTY;
  auto doc = arangodb::velocypack::Parser::fromJson("{ \"str\": \"abc\", \"value\": 10 }");
  std::string collectionName("testCollection");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\":\"" + collectionName + "\"}");
  auto logicalCollection = vocbase().createCollection(collectionJson->slice());
  ASSERT_TRUE(logicalCollection);
  size_t const columnsCount = 6; // PK + storedValues
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \
        \"id\": 42, \
        \"name\": \"testView\", \
        \"type\": \"arangosearch\", \
        \"storedValues\": [{\"fields\":[\"str\"]}, {\"fields\":[\"foo\"]}, {\"fields\":[\"value\"]},\
                          {\"fields\":[\"_id\"]}, {\"fields\":[\"str\", \"foo\", \"value\"]}] \
      }");
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase().createView(viewJson->slice()));
  ASSERT_TRUE(view);

  auto updateJson = VPackParser::fromJson(
    "{\"links\": {\"" + collectionName + "\": {\"includeAllFields\": true} }}");
  EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());

  arangodb::velocypack::Builder builder;

  builder.openObject();
  view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
  builder.close();

  auto slice = builder.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE(slice.get("type").copyString() ==
              arangodb::iresearch::DATA_SOURCE_TYPE.name());
  EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
  auto tmpSlice = slice.get("links");
  EXPECT_TRUE(tmpSlice.isObject() && 1 == tmpSlice.length());

  arangodb::ManagedDocumentResult insertedDoc;
  {
    arangodb::OperationOptions opt;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase()),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    auto const res =
        logicalCollection->insert(&trx, doc->slice(), insertedDoc, opt);
    EXPECT_TRUE(res.ok());

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *view)
                ->commit().ok());
  }

  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase()),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *view);
    ASSERT_TRUE(link);
    irs::directory_reader const snapshotReader = link->snapshot();
    std::string const columns[] = {arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("_id"),
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("foo"),
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("foo") +
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + "str" +
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + "value",
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("str"),
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("value"),
                                   "@_PK"};
    for (auto const& segment : snapshotReader) {
      auto col = segment.columns();
      auto doc = segment.docs_iterator();
      ASSERT_TRUE(doc);
      ASSERT_TRUE(doc->next());
      size_t counter = 0;
      while (col->next()) {
        auto const& val = col->value();
        ASSERT_TRUE(counter < columnsCount);
        EXPECT_EQ(columns[counter], val.name);
        if (5 == counter) { // skip PK
          ++counter;
          continue;
        }
        auto columnReader = segment.column_reader(val.id);
        ASSERT_TRUE(columnReader);
        auto valReader = columnReader->values();
        ASSERT_TRUE(valReader);
        irs::bytes_ref value; // column value
        ASSERT_TRUE(valReader(doc->value(), value));
        if (1 == counter) { // foo
          EXPECT_TRUE(value.null());
          ++counter;
          continue;
        }
        size_t valueSize = value.size();
        auto slice = VPackSlice(value.c_str());
        switch (counter) {
        case 0: {
          ASSERT_TRUE(slice.isString());
          arangodb::velocypack::ValueLength length = 0;
          auto str = slice.getString(length);
          std::string strVal(str, length);
          ASSERT_TRUE(length > collectionName.size());
          EXPECT_EQ(collectionName + "/", strVal.substr(0, collectionName.size() + 1));
          break;
        }
        case 2: {
          arangodb::velocypack::ValueLength size = slice.byteSize();
          ASSERT_TRUE(slice.isString());
          arangodb::velocypack::ValueLength length = 0;
          auto str = slice.getString(length);
          EXPECT_EQ("abc", std::string(str, length));
          slice = VPackSlice(slice.start() + slice.byteSize());
          size += slice.byteSize();
          EXPECT_TRUE(slice.isNull());
          slice = VPackSlice(slice.start() + slice.byteSize());
          size += slice.byteSize();
          ASSERT_TRUE(slice.isNumber());
          EXPECT_EQ(10, slice.getNumber<int>());
          EXPECT_EQ(valueSize, size);
          break;
        }
        case 3: {
          ASSERT_TRUE(slice.isString());
          arangodb::velocypack::ValueLength length = 0;
          auto str = slice.getString(length);
          EXPECT_EQ("abc", std::string(str, length));
          break;
        }
        case 4:
          ASSERT_TRUE(slice.isNumber());
          EXPECT_EQ(10, slice.getNumber<int>());
          break;
        default:
          ASSERT_TRUE(false);
          break;
        }
        ++counter;
      }
      EXPECT_EQ(columnsCount, counter);
    }
  }
}

TEST_F(IResearchQueryNoMaterializationTest, testStoredValuesRecordWithCompression) {
  static std::vector<std::string> const EMPTY;
  auto doc = arangodb::velocypack::Parser::fromJson("{ \"str\": \"abc\", \"value\": 10 }");
  std::string collectionName("testCollection");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
    "{ \"name\":\"" + collectionName + "\"}");
  auto logicalCollection = vocbase().createCollection(collectionJson->slice());
  ASSERT_TRUE(logicalCollection);
  size_t const columnsCount = 6; // PK + storedValues
  auto viewJson = arangodb::velocypack::Parser::fromJson(
    "{ \
        \"id\": 42, \
        \"name\": \"testView\", \
        \"type\": \"arangosearch\", \
        \"storedValues\": [{\"fields\":[\"str\"], \"compression\":\"none\"}, [\"foo\"],\
        {\"fields\":[\"value\"], \"compression\":\"lz4\"}, [\"_id\"], {\"fields\":[\"str\", \"foo\", \"value\"]}] \
      }");
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase().createView(viewJson->slice()));
  ASSERT_TRUE(view);

  auto updateJson = VPackParser::fromJson(
    "{\"links\": {\"" + collectionName + "\": {\"includeAllFields\": true} }}");
  EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());

  arangodb::velocypack::Builder builder;

  builder.openObject();
  view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
  builder.close();

  auto slice = builder.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE(slice.get("type").copyString() ==
    arangodb::iresearch::DATA_SOURCE_TYPE.name());
  EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
  auto tmpSlice = slice.get("links");
  EXPECT_TRUE(tmpSlice.isObject() && 1 == tmpSlice.length());

  arangodb::ManagedDocumentResult insertedDoc;
  {
    arangodb::OperationOptions opt;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase()),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    auto const res =
      logicalCollection->insert(&trx, doc->slice(), insertedDoc, opt);
    EXPECT_TRUE(res.ok());

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *view)
      ->commit().ok());
  }

  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase()),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *view);
    ASSERT_TRUE(link);
    irs::directory_reader const snapshotReader = link->snapshot();
    std::string const columns[] = { arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("_id"),
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("foo"),
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("foo") +
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + "str" +
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + "value",
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("str"),
                                   arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("value"),
                                   "@_PK" };
    for (auto const& segment : snapshotReader) {
      auto col = segment.columns();
      auto doc = segment.docs_iterator();
      ASSERT_TRUE(doc);
      ASSERT_TRUE(doc->next());
      size_t counter = 0;
      while (col->next()) {
        auto const& val = col->value();
        ASSERT_TRUE(counter < columnsCount);
        EXPECT_EQ(columns[counter], val.name);
        if (5 == counter) { // skip PK
          ++counter;
          continue;
        }
        auto columnReader = segment.column_reader(val.id);
        ASSERT_TRUE(columnReader);
        auto valReader = columnReader->values();
        ASSERT_TRUE(valReader);
        irs::bytes_ref value; // column value
        ASSERT_TRUE(valReader(doc->value(), value));
        if (1 == counter) { // foo
          EXPECT_TRUE(value.null());
          ++counter;
          continue;
        }
        size_t valueSize = value.size();
        auto slice = VPackSlice(value.c_str());
        switch (counter) {
        case 0: {
          ASSERT_TRUE(slice.isString());
          arangodb::velocypack::ValueLength length = 0;
          auto str = slice.getString(length);
          std::string strVal(str, length);
          ASSERT_TRUE(length > collectionName.size());
          EXPECT_EQ(collectionName + "/", strVal.substr(0, collectionName.size() + 1));
          break;
        }
        case 2: {
          arangodb::velocypack::ValueLength size = slice.byteSize();
          ASSERT_TRUE(slice.isString());
          arangodb::velocypack::ValueLength length = 0;
          auto str = slice.getString(length);
          EXPECT_EQ("abc", std::string(str, length));
          slice = VPackSlice(slice.start() + slice.byteSize());
          size += slice.byteSize();
          EXPECT_TRUE(slice.isNull());
          slice = VPackSlice(slice.start() + slice.byteSize());
          size += slice.byteSize();
          ASSERT_TRUE(slice.isNumber());
          EXPECT_EQ(10, slice.getNumber<int>());
          EXPECT_EQ(valueSize, size);
          break;
        }
        case 3: {
          ASSERT_TRUE(slice.isString());
          arangodb::velocypack::ValueLength length = 0;
          auto str = slice.getString(length);
          EXPECT_EQ("abc", std::string(str, length));
          break;
        }
        case 4:
          ASSERT_TRUE(slice.isNumber());
          EXPECT_EQ(10, slice.getNumber<int>());
          break;
        default:
          ASSERT_TRUE(false);
          break;
        }
        ++counter;
      }
      EXPECT_EQ(columnsCount, counter);
    }
  }
}
