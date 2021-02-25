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

#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/QueryRegistry.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchView.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include "search/boolean_filter.hpp"
#include "search/range_filter.hpp"
#include "search/term_filter.hpp"

#include <velocypack/Iterator.h>

extern const char* ARGV0;  // defined in main.cpp

namespace {

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();

bool findEmptyNodes(TRI_vocbase_t& vocbase, std::string const& queryString,
                    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr) {
  auto options = VPackParser::fromJson(
      //    "{ \"tracing\" : 1 }"
      "{ }");

  arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase), arangodb::aql::QueryString(queryString),
                             bindVars, options->slice());

  query.prepareQuery(arangodb::aql::SerializationFormat::SHADOWROWS);

  arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

  // try to find `EnumerateViewNode`s and process corresponding filters and sorts
  query.plan()->findNodesOfType(nodes, arangodb::aql::ExecutionNode::NORESULTS, true);
  return !nodes.empty();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQueryOptimizationTest : public IResearchQueryTest {
 protected:
  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  void addLinkToCollection(std::shared_ptr<arangodb::iresearch::IResearchView>& view) {
    auto updateJson = VPackParser::fromJson(
        "{ \"links\" : {"
        "\"collection_1\" : { \"includeAllFields\" : true }"
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
    EXPECT_TRUE(tmpSlice.isObject() && 1 == tmpSlice.length());
  }

  void SetUp() override {
    auto createJson = VPackParser::fromJson(
        "{ \
        \"name\": \"testView\", \
        \"type\": \"arangosearch\" \
      }");

    std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
    std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;

    // add collection_1
    {
      auto collectionJson =
          VPackParser::fromJson("{ \"name\": \"collection_1\" }");
      logicalCollection1 = vocbase().createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection1);
    }

    // add view
    auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
        vocbase().createView(createJson->slice()));
    ASSERT_FALSE(!view);

    // add link to collection
    addLinkToCollection(view);

    // populate view with the data
    {
      arangodb::OperationOptions opt;
      static std::vector<std::string> const EMPTY;
      arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(vocbase()),
          EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collection
      auto builder = VPackParser::fromJson(
          "[{ \"values\" : [ \"A\", \"C\", \"B\" ] }]");

      auto root = builder->slice();
      ASSERT_TRUE(root.isArray());

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocs.emplace_back();
        auto const res =
            logicalCollection1->insert(&trx, doc, insertedDocs.back(), opt);
        EXPECT_TRUE(res.ok());
      }

      EXPECT_TRUE(trx.commit().ok());
      EXPECT_TRUE((arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection1, *view)
                       ->commit()
                       .ok()));
    }
  }
};

std::vector<std::string> optimizerOptionsAvailable = {
  "",
  " OPTIONS {\"conditionOptimization\":\"auto\"} ",
  " OPTIONS {\"conditionOptimization\":\"nodnf\"} ",
  " OPTIONS {\"conditionOptimization\":\"noneg\"} ",
  " OPTIONS {\"conditionOptimization\":\"none\"} "
};

constexpr size_t disabledDnfOptimizationStart = 2;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

// dedicated to https://github.com/arangodb/arangodb/issues/8294
  // a IN [ x ] && a == y, x < y
TEST_F(IResearchQueryOptimizationTest, test_1) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);

    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values == 'C' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));

    // for all optimization modes query should be the same
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }


    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a == y, x == y
TEST_F(IResearchQueryOptimizationTest, test_2) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C', 'B', 'A' ] AND d.values "
        "== 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a == y, x > y
TEST_F(IResearchQueryOptimizationTest, test_3) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C', 'B' ] AND d.values == 'A' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a == y, x < y
TEST_F(IResearchQueryOptimizationTest, test_4) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values != 'D' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      }
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }
   

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a == y, x < y
TEST_F(IResearchQueryOptimizationTest, test_5) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values != 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      }
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a == y, x > y
TEST_F(IResearchQueryOptimizationTest, test_6) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C', 'D' ] AND d.values != 'D' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
      }
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  /*
  //FIXME
  // a IN [ x ] && a == y, x == y
TEST_F(IResearchQueryOptimizationTest, test_7) {
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'A', 'A' ] AND d.values != 'A'") + o +) + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
    vocbase(), query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

  EXPECT_TRUE(findEmptyNodes(vocbase(), query));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

  auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }
  */

  // a IN [ x ] && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_8) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C', 'B' ] AND d.values != 'A' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_9) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C', 'B' ] AND d.values != '@' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a < y, x < y
TEST_F(IResearchQueryOptimizationTest, test_10) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'A', 'B' ] AND d.values < 'C' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a < y, x == y
TEST_F(IResearchQueryOptimizationTest, test_11) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'A', 'C' ] AND d.values < 'C' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a < y, x > y
TEST_F(IResearchQueryOptimizationTest, test_12) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'D', 'C' ] AND d.values < 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a <= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_13) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);

    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'B', 'C' ] AND d.values <= 'D' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a IN [ x ] && a <= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_14) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'B', 'C' ] AND d.values <= 'C' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a IN [ x ] && a <= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_15) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'B', 'C' ] AND d.values <= 'A' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a IN [ x ] && a >= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_16) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values >= 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a >= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_17) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);

    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values >= 'A' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a IN [ x ] && a >= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_18) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C', 'D' ] AND d.values >= 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a > y, x < y
TEST_F(IResearchQueryOptimizationTest, test_19) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);

    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values > 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a > y, x == y
TEST_F(IResearchQueryOptimizationTest, test_20) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C', 'B' ] AND d.values > 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a > y, x > y
TEST_F(IResearchQueryOptimizationTest, test_21) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C', 'D' ] AND d.values > 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a IN [ y ]
TEST_F(IResearchQueryOptimizationTest, test_22) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'A', 'B' ] AND d.values IN [ "
        "'A', 'B', 'C' ]") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));

    // FIXME optimize
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      {
        auto& sub = root.add<irs::Or>();
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = sub.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a == y, x < y
TEST_F(IResearchQueryOptimizationTest, test_23) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'B' ] AND d.values == 'C' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a IN [ x ] && a == y, x == y
TEST_F(IResearchQueryOptimizationTest, test_24) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values == 'C' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a == y, x > y
TEST_F(IResearchQueryOptimizationTest, test_25) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values == 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a IN [ x ] && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_26) {

  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'A' ] AND d.values != 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a IN [ x ] && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_27) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values != 'C' ") + o + "RETURN d";

    if (optimizeType < disabledDnfOptimizationStart) {
      EXPECT_TRUE(findEmptyNodes(vocbase(), query));
    } else {
      // no optimization will give us redundant nodes, but that is expected 
      EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
        { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));
      EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    }

    // check structure
    {
      if (optimizeType < disabledDnfOptimizationStart) {
        irs::And expected;
        auto& root = expected;
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        assertFilterOptimized(vocbase(), query, expected);
      } else {
        irs::Or expected;
        auto& root = expected.add<irs::And>();
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }

        assertFilterOptimized(vocbase(), query, expected);
      }
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a IN [ x ] && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_28) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN ['B'] AND d.values != 'C'" ) + o + " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}
  // a IN [ x ] && a < y, x < y
TEST_F(IResearchQueryOptimizationTest, test_29) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'B' ] AND d.values < 'C' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a IN [ x ] && a < y, x == y
TEST_F(IResearchQueryOptimizationTest, test_30) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values < 'C' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a < y, x > y
TEST_F(IResearchQueryOptimizationTest, test_31) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values < 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a IN [ x ] && a <= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_32) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'B' ] AND d.values <= 'C' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      } else{
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a IN [x] && a <= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_33) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'B' ] AND d.values <= 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a IN [ x ] && a <= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_34) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values <= 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a IN [ x ] && a >= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_35) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'A' ] AND d.values >= 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a IN [ x ] && a >= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_36) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN [ 'B' ] AND d.values >= 'B' ") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}
  // a IN [x] && a >= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_37) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN ['C'] AND d.values >= 'B'") + o + " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}
  // a IN [x] && a > y, x < y
TEST_F(IResearchQueryOptimizationTest, test_38) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN ['A'] AND d.values > 'B'") + o + " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}
  // a IN [x] && a > y, x == y
TEST_F(IResearchQueryOptimizationTest, test_39) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN ['B'] AND d.values > 'B'") + o + " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a IN [x] && a > y, x > y
TEST_F(IResearchQueryOptimizationTest, test_40) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values IN ['C'] AND d.values > 'B'") + o + " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a == x && a == y, x < y
TEST_F(IResearchQueryOptimizationTest, test_41) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'B' AND d.values == 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a == x && a == y, x == y
  TEST_F(IResearchQueryOptimizationTest, test_42) {
    for (auto& o : optimizerOptionsAvailable) {
      SCOPED_TRACE(o);
      std::string const query =
        std::string("FOR d IN testView SEARCH d.values == 'C' AND d.values == 'C'") + o + "RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
        { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

      EXPECT_FALSE(findEmptyNodes(vocbase(), query));
      // check structure
      {
        irs::Or expected;
        auto& root = expected.add<irs::And>();
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        assertFilterOptimized(vocbase(), query, expected);
      }

      std::vector<arangodb::velocypack::Slice> expectedDocs{
          arangodb::velocypack::Slice(insertedDocs[0].vpack()),
      };

      auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
  }

// a == x && a == y, x > y
TEST_F(IResearchQueryOptimizationTest, test_43) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'C' AND d.values == 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
        { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a == x && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_44) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'A' AND d.values != 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
        { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a == x && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_45) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'C' AND d.values != 'C'") + o + "RETURN d";
    if (optimizeType < disabledDnfOptimizationStart) {
      // FIXME EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      //  { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));
      EXPECT_TRUE(findEmptyNodes(vocbase(), query));
    } else {
      EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
        { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));
      EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    }
    // check structure
    {
      if (optimizeType < disabledDnfOptimizationStart) {
        irs::And expected;
        auto& root = expected; 
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }

        assertFilterOptimized(vocbase(), query, expected);
      } else {
        irs::Or expected;
        auto& root = expected.add<irs::And>();
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }

        assertFilterOptimized(vocbase(), query, expected);
      }
    }
    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    optimizeType++;
  }
}

  // a == x && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_46) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'B' AND d.values != 'C'")  + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
        { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a == x && a < y, x < y
TEST_F(IResearchQueryOptimizationTest, test_47) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'B' AND d.values < 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
        { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a == x && a < y, x == y
TEST_F(IResearchQueryOptimizationTest, test_48) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'C' AND d.values < 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a == x && a < y, x > y
TEST_F(IResearchQueryOptimizationTest, test_49) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'C' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a == x && a <= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_50) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'B' AND d.values <= 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a == x && a <= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_51) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'B' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a == x && a <= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_52) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'C' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a == x && a >= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_53) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'A' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a == x && a >= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_54) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'B' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a == x && a >= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_55) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'C' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a == x && a > y, x < y
TEST_F(IResearchQueryOptimizationTest, test_56) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'A' AND d.values > 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}
  // a == x && a > y, x == y
TEST_F(IResearchQueryOptimizationTest, test_57) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'B' AND d.values > 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      } else {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a == x && a > y, x > y
TEST_F(IResearchQueryOptimizationTest, test_58) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values == 'C' AND d.values > 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a != x && a == y, x < y
TEST_F(IResearchQueryOptimizationTest, test_59) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != '@' AND d.values == 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a == y, x < y
TEST_F(IResearchQueryOptimizationTest, test_60) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'A' AND d.values == 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a == y, x == y
TEST_F(IResearchQueryOptimizationTest, test_61) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);

    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'A' AND d.values == 'A'") + o + "RETURN d";
    if (optimizeType < disabledDnfOptimizationStart) {
      // FIXME EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      //  { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));
      EXPECT_TRUE(findEmptyNodes(vocbase(), query));
    } else {
      EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
        { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

      EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    }
    // check structure
    {
      if (optimizeType < disabledDnfOptimizationStart) {
        irs::And expected;
        auto& root = expected; 
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }

        assertFilterOptimized(vocbase(), query, expected);
      } else {
        irs::Or expected;
        auto& root = expected.add<irs::And>();
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }

        assertFilterOptimized(vocbase(), query, expected);
      }
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a != x && a == y, x > y
TEST_F(IResearchQueryOptimizationTest, test_62) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'D' AND d.values == 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a == y, x > y
TEST_F(IResearchQueryOptimizationTest, test_63) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'B' AND d.values == 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_64) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != '@' AND d.values != 'D'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
      }
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_65) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'A' AND d.values != 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a != x && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_66) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'D' AND d.values != 'D'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_67) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'A' AND d.values != 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_68) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'B' AND d.values != 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a != x && a < y, x < y
TEST_F(IResearchQueryOptimizationTest, test_69) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != '0' AND d.values < 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("0"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a < y, x == y
TEST_F(IResearchQueryOptimizationTest, test_70) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'A' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a < y, x == y
TEST_F(IResearchQueryOptimizationTest, test_71) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != '@' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a < y, x == y
TEST_F(IResearchQueryOptimizationTest, test_72) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'D' AND d.values < 'D'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a < y, x > y
TEST_F(IResearchQueryOptimizationTest, test_73) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'D' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a < y, x > y
TEST_F(IResearchQueryOptimizationTest, test_74) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'C' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a <= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_75) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != '0' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("0"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a != x && a <= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_76) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'A' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();      
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a <= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_77) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'D' AND d.values <= 'D'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a <= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_78) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'B' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a <= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_79) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'D' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a <= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_80) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'C' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a >= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_81) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != '0' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("0"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a >= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_82) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'A' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a >= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_83) {

  for (auto& o : optimizerOptionsAvailable) {
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != '0' AND d.values >= '0'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    SCOPED_TRACE(o);
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("0"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("0"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a >= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_84) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'A' AND d.values >= 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a >= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_85) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'D' AND d.values >= 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a >= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_86) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'C' AND d.values >= 'A'") + o + "RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
        { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

      EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a > y, x < y
TEST_F(IResearchQueryOptimizationTest, test_87) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != '0' AND d.values > 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("0"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a > y, x < y
TEST_F(IResearchQueryOptimizationTest, test_88) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'A' AND d.values > 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a > y, x == y
TEST_F(IResearchQueryOptimizationTest, test_89) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != '0' AND d.values > '0'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("0"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("0"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a > y, x == y
TEST_F(IResearchQueryOptimizationTest, test_90) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'A' AND d.values > 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a > y, x > y
TEST_F(IResearchQueryOptimizationTest, test_91) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'D' AND d.values > 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a != x && a > y, x > y
TEST_F(IResearchQueryOptimizationTest, test_92) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values != 'C' AND d.values > 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::Not>().filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a < x && a == y, x < y
TEST_F(IResearchQueryOptimizationTest, test_93) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values == 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }

      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    optimizeType++;
  }
}

  // a < x && a == y, x == y
TEST_F(IResearchQueryOptimizationTest, test_94) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values == 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a == y, x > y
TEST_F(IResearchQueryOptimizationTest, test_95) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'C' AND d.values == 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      }

      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_96) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values != 'D'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_97) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values != 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_98) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'D' AND d.values != 'D'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_99) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values != 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_100) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'C' AND d.values != '0'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("0"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }

      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("0"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_101) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'C' AND d.values != 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a < y, x < y
TEST_F(IResearchQueryOptimizationTest, test_102) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values < 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a < y, x == y
TEST_F(IResearchQueryOptimizationTest, test_103) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a < x && a < y, x > y
TEST_F(IResearchQueryOptimizationTest, test_104) {

  std::vector<arangodb::velocypack::Slice> expectedDocs{
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
  };
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'C' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}
  // a < x && a <= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_105) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values <= 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a <= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_106) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'C' AND d.values <= 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a <= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_107) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'C' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a >= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_108) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values >= 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a >= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_109) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a >= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_110) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'C' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a > y, x < y
TEST_F(IResearchQueryOptimizationTest, test_111) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values > 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a > y, x == y
TEST_F(IResearchQueryOptimizationTest, test_112) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'B' AND d.values > 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a < x && a > y, x > y
TEST_F(IResearchQueryOptimizationTest, test_113) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values < 'C' AND d.values > 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a == y, x < y
TEST_F(IResearchQueryOptimizationTest, test_114) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'A' AND d.values == 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a == y, x == y
TEST_F(IResearchQueryOptimizationTest, test_115) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'A' AND d.values == 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a == y, x > y
TEST_F(IResearchQueryOptimizationTest, test_116) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'B' AND d.values == 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_117) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'A' AND d.values != 'D'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_118) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'A' AND d.values != 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_119) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'B' AND d.values != 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_120) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'D' AND d.values != 'D'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_121) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'C' AND d.values != '@'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_122) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'C' AND d.values != 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a < y, x < y
TEST_F(IResearchQueryOptimizationTest, test_123) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'A' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a < y, x == y
TEST_F(IResearchQueryOptimizationTest, test_124) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'B' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}
  // a <= x && a < y, x > y
TEST_F(IResearchQueryOptimizationTest, test_125) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'C' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a <= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_126) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'A' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a <= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_127) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'B' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a <= x && a <= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_128) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'C' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a >= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_129) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'A' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a >= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_130) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'A' AND d.values >= 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a >= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_131) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'C' AND d.values >= 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a > y, x < y
TEST_F(IResearchQueryOptimizationTest, test_132) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'A' AND d.values > 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a > y, x == y
TEST_F(IResearchQueryOptimizationTest, test_133) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'A' AND d.values > 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a <= x && a > y, x > y
TEST_F(IResearchQueryOptimizationTest, test_134) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values <= 'C' AND d.values > 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
          filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a >= x && a == y, x < y
TEST_F(IResearchQueryOptimizationTest, test_135) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'A' AND d.values == 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}
  // a >= x && a == y, x == y
TEST_F(IResearchQueryOptimizationTest, test_136) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'A' AND d.values == 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a >= x && a == y, x > y
TEST_F(IResearchQueryOptimizationTest, test_137) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'B' AND d.values == 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a >= x && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_138) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'A' AND d.values != 'D'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a >= x && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_139) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'A' AND d.values != 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a >= x && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_140) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= '@' AND d.values != '@'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a >= x && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_141) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'A' AND d.values != 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}
  // a >= x && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_142) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'B' AND d.values != 'D'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a >= x && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_143) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'B' AND d.values != 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a >= x && a < y, x < y
TEST_F(IResearchQueryOptimizationTest, test_144) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'A' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a >= x && a < y, x == y
TEST_F(IResearchQueryOptimizationTest, test_145) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'B' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a >= x && a < y, x > y
TEST_F(IResearchQueryOptimizationTest, test_146) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'C' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a >= x && a <= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_147) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'A' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a >= x && a <= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_148) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'B' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}
  // a >= x && a <= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_149) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'C' AND d.values <= 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a >= x && a >= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_150) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'A' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a >= x && a >= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_151) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'B' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a >= x && a >= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_152) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'C' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a >= x && a > y, x < y
TEST_F(IResearchQueryOptimizationTest, test_153) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'A' AND d.values > 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a >= x && a > y, x == y
TEST_F(IResearchQueryOptimizationTest, test_154) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'B' AND d.values > 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a >= x && a > y, x > y
TEST_F(IResearchQueryOptimizationTest, test_155) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values >= 'B' AND d.values > 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a > x && a == y, x < y
TEST_F(IResearchQueryOptimizationTest, test_156) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'A' AND d.values == 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a > x && a == y, x == y
TEST_F(IResearchQueryOptimizationTest, test_157) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'B' AND d.values == 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a > x && a == y, x > y
TEST_F(IResearchQueryOptimizationTest, test_158) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'B' AND d.values == 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a > x && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_159) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'A' AND d.values != 'D'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("D"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a > x && a != y, x < y
TEST_F(IResearchQueryOptimizationTest, test_160) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'A' AND d.values != 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a > x && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_161) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > '@' AND d.values != '@'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a > x && a != y, x == y
TEST_F(IResearchQueryOptimizationTest, test_162) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'A' AND d.values != 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    optimizeType++;
  }
}

  // a > x && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_163) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'B' AND d.values != '@'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("@"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a > x && a != y, x > y
TEST_F(IResearchQueryOptimizationTest, test_164) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'B' AND d.values != 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType < disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      } else {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
        {
          auto& filter = root.add<irs::Not>().filter<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a > x && a < y, x < y
TEST_F(IResearchQueryOptimizationTest, test_165) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'A' AND d.values < 'C'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a > x && a < y, x == y
TEST_F(IResearchQueryOptimizationTest, test_166) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'B' AND d.values < 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));
    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a > x && a < y, x > y
TEST_F(IResearchQueryOptimizationTest, test_167) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'C' AND d.values < 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{};

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a > x && a <= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_168) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'A' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a > x && a <= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_169) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'B' AND d.values <= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a > x && a <= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_170) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'B' AND d.values <= 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a > x && a >= y, x < y
TEST_F(IResearchQueryOptimizationTest, test_171) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'A' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a > x && a >= y, x == y
TEST_F(IResearchQueryOptimizationTest, test_172) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'B' AND d.values >= 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a > x && a >= y, x > y
TEST_F(IResearchQueryOptimizationTest, test_173) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'B' AND d.values >= 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a > x && a > y, x < y
TEST_F(IResearchQueryOptimizationTest, test_174) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'A' AND d.values > 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

  // a > x && a > y, x == y
TEST_F(IResearchQueryOptimizationTest, test_175) {
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'B' AND d.values > 'B'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

  // a > x && a > y, x > y
TEST_F(IResearchQueryOptimizationTest, test_176) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH d.values > 'B' AND d.values > 'A'") + o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      }
      if (optimizeType >= disabledDnfOptimizationStart) {
        {
          auto& filter = root.add<irs::by_range>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
          filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        }
      }
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

// check double negation is always collapsed
TEST_F(IResearchQueryOptimizationTest, test_177) {

  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);

    std::string const query =
      std::string("FOR d IN testView SEARCH  NOT( NOT (d.values == 'B'))") + o + "RETURN d";
     
    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

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
}

// check DNF conversion disabled 
TEST_F(IResearchQueryOptimizationTest, test_178) {
  auto dnfConvertedExpected = [](irs::Or& expected) {
    auto& root = expected;
    // left part B && C
    {
      auto& andFilter = root.add<irs::And>();
      {
        auto& filter = andFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      {
        auto& filter = andFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
    }
    // right part B && A
    {
      auto& andFilter = root.add<irs::And>();
      {
        auto& filter = andFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      {
        auto& filter = andFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
    }
  };

  auto nonConvertedExpected = [](irs::Or& expected) {
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("values");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
    }

    {
      auto& sub = root.add<irs::Or>();
      {
        auto& filter = sub.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
      {
        auto& filter = sub.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
    }
  };

  std::vector<std::function<void(irs::Or&)>> structureChecks =
      { dnfConvertedExpected, dnfConvertedExpected, nonConvertedExpected, nonConvertedExpected, nonConvertedExpected };
  ASSERT_EQ(structureChecks.size(), optimizerOptionsAvailable.size());

  size_t structCheckIdx = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH  d.values == 'B' AND  ( d.values == 'C'  OR d.values == 'A' ) ") +
      o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      structureChecks[structCheckIdx](expected);
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());

    structCheckIdx++;
  }
}

// check DNF conversion disabled  but IN nodes processed (sorted and deduplicated)!
TEST_F(IResearchQueryOptimizationTest, test_179) {
  auto dnfConvertedExpected = [](irs::Or& expected) {
    auto& root = expected;
    {
      auto& andFilter = root.add<irs::And>();
      {
        auto& part = andFilter.add<irs::Or>();
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& part = andFilter.add<irs::Or>();
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
    }
    {
      auto& andFilter = root.add<irs::And>();
      {
        auto& part = andFilter.add<irs::Or>();
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& part = andFilter.add<irs::Or>();
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
    }
  };

  auto nonConvertedExpected = [](irs::Or& expected) {
    auto& root = expected.add<irs::And>();
    {
      auto& sub = root.add<irs::Or>();
      {
        auto& filter = sub.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = sub.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
    }
    {
      auto& filter = root.add<irs::Or>();
      {
        auto& or2 = filter.add<irs::Or>();
        {
          auto& filter = or2.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = or2.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& or2 = filter .add<irs::Or>();
        {
          auto& filter = or2.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = or2.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
    }
  };

  std::vector<std::function<void(irs::Or&)>> structureChecks =
  { dnfConvertedExpected, dnfConvertedExpected, nonConvertedExpected, nonConvertedExpected, nonConvertedExpected };
  ASSERT_EQ(structureChecks.size(), optimizerOptionsAvailable.size());

  size_t structCheckIdx = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH  d.values IN ['A', 'C'] AND  ( d.values IN ['C', 'B', 'C']  OR d.values IN ['A', 'B'] ) ") +
      o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      structureChecks[structCheckIdx](expected);
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());

    structCheckIdx++;
  }
}

// check DNF conversion disabled (with root disjunction)  but IN nodes processed (sorted and deduplicated)!
TEST_F(IResearchQueryOptimizationTest, test_180) {
  auto dnfConvertedExpected = [](irs::Or& expected) {
    auto& root = expected;
    {
      {
        auto& andFilter = root.add<irs::And>();
        auto& part = andFilter.add<irs::Or>();
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& andFilter = root.add<irs::And>();
        auto& part = andFilter.add<irs::Or>();
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& andFilter = root.add<irs::And>();
        auto& part = andFilter.add<irs::Or>();
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
    }
  };

  auto nonConvertedExpected = [](irs::Or& expected) {
    auto& root = expected;
    {
      auto& sub = root.add<irs::And>().add<irs::Or>();
      {
        auto& filter = sub.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = sub.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
    }
    {
      auto& orFilter = root.add<irs::And>().add<irs::Or>();
      {
        auto& filter = orFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      {
        auto& filter = orFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
    }
    {
      auto& orFilter = root.add<irs::And>().add<irs::Or>();
      {
        auto& filter = orFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = orFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
    }
  };

  std::vector<std::function<void(irs::Or&)>> structureChecks =
  { dnfConvertedExpected, dnfConvertedExpected, nonConvertedExpected, nonConvertedExpected, nonConvertedExpected };
  ASSERT_EQ(structureChecks.size(), optimizerOptionsAvailable.size());

  size_t structCheckIdx = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH  d.values IN ['A', 'C'] OR  ( d.values IN ['C', 'B', 'C']  OR d.values IN ['A', 'B'] ) ") +
      o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      structureChecks[structCheckIdx](expected);
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());

    structCheckIdx++;
  }
}

// check DNF conversion disabled (with root disjunction and conjunction inside)  but IN nodes processed (sorted and deduplicated)!
TEST_F(IResearchQueryOptimizationTest, test_181) {
  auto dnfConvertedExpected = [](irs::Or& expected) {
    auto& root = expected;
    {
      auto& orFilter = root.add<irs::And>().add<irs::Or>();
      {
        auto& filter = orFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = orFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
    }
    {
      auto& andFilter = root.add<irs::And>();
      {
        auto& part = andFilter.add<irs::Or>();
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& part = andFilter.add<irs::Or>();
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = part.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
    }
  };

  auto nonConvertedExpected = [](irs::Or& expected) {
    {
      auto& orFilter = expected.add<irs::And>().add<irs::Or>();
      {
        auto& filter = orFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = orFilter.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }
    }
    {
      auto& andFilter = expected.add<irs::And>();
      {
        auto& orFilter = andFilter.add<irs::Or>();
        {
          auto& filter = orFilter.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
        {
          auto& filter = orFilter.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
        }
      }
      {
        auto& orFilter = andFilter.add<irs::Or>();
        {
          auto& filter = orFilter.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
        }
        {
          auto& filter = orFilter.add<irs::by_term>();
          *filter.mutable_field() = mangleStringIdentity("values");
          filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
        }
      }
    }
  };

  std::vector<std::function<void(irs::Or&)>> structureChecks =
  { dnfConvertedExpected, dnfConvertedExpected, nonConvertedExpected, nonConvertedExpected, nonConvertedExpected };
  ASSERT_EQ(structureChecks.size(), optimizerOptionsAvailable.size());

  size_t structCheckIdx = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH  d.values IN ['A', 'C'] OR  ( d.values IN ['C', 'B', 'C']  AND d.values IN ['A', 'B'] ) ") +
      o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      structureChecks[structCheckIdx](expected);
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{
        arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());

    structCheckIdx++;
  }
}


// check Negation conversion disabled 
TEST_F(IResearchQueryOptimizationTest, test_182) {
  auto negationConvertedExpected = [](irs::Or& expected) {
    auto& root = expected;
    {
      auto& notFilter = root.add<irs::And>().add<irs::Not>();
      {
        auto& filter = notFilter.filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
    }
    {
      auto& notFilter = root.add<irs::And>().add<irs::Not>();
      {
        auto& filter = notFilter.filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
    }
  };

  auto nonConvertedExpected = [](irs::Or& expected) {
    auto& root = expected.add<irs::And>().add<irs::Not>().filter<irs::And>().add<irs::And>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("values");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("values");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
    }
  };

  std::vector<std::function<void(irs::Or&)>> structureChecks =
  { negationConvertedExpected, negationConvertedExpected, negationConvertedExpected, nonConvertedExpected, nonConvertedExpected };
  ASSERT_EQ(structureChecks.size(), optimizerOptionsAvailable.size());

  size_t structCheckIdx = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH  NOT (d.values == 'A' AND  d.values == 'B') ") +
      o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      structureChecks[structCheckIdx](expected);
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{

    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());

    structCheckIdx++;
  }
}

// check Negation conversion disabled 
TEST_F(IResearchQueryOptimizationTest, test_183) {
  auto negationConvertedExpected = [](irs::Or& expected) {
    auto& root = expected.add<irs::And>();
    {
      auto& notFilter = root.add<irs::Not>();
      {
        auto& filter = notFilter.filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
    }
    {
      auto& notFilter = root.add<irs::Not>();
      {
        auto& filter = notFilter.filter<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
    }
  };

  auto nonConvertedExpected = [](irs::Or& expected) {
    auto& root = expected.add<irs::And>().add<irs::Not>().filter<irs::And>().add<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("values");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("values");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
    }
  };

  std::vector<std::function<void(irs::Or&)>> structureChecks =
  { negationConvertedExpected, negationConvertedExpected, negationConvertedExpected, nonConvertedExpected, nonConvertedExpected };
  ASSERT_EQ(structureChecks.size(), optimizerOptionsAvailable.size());

  size_t structCheckIdx = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH  NOT (d.values == 'A' OR  d.values == 'B') ") +
      o + "RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure
    {
      irs::Or expected;
      structureChecks[structCheckIdx](expected);
      assertFilterOptimized(vocbase(), query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs{

    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());

    structCheckIdx++;
  }
}

// check OR deduplication in sub-nodes
TEST_F(IResearchQueryOptimizationTest, test_184) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH  (d.values == 'A' OR d.values == 'B' OR d.values == 'A') AND  (d.values == 'A' OR d.values == 'C' OR d.values == 'C') ") +
      o + "RETURN d";
    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure only for non-optimized
    // Dnf-converter filter is out of scope, just run it and verify
    // returned documents are the same
    if (optimizeType >= disabledDnfOptimizationStart) {
      irs::Or expected;
      auto& andFilter = expected.add<irs::And>();
      auto& left = andFilter.add<irs::Or>();
      {
        auto& filter = left.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = left.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      auto& right = andFilter.add<irs::Or>();
      {
        auto& filter = right.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = right.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }
    std::vector<arangodb::velocypack::Slice> expectedDocs{
       arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}

// check IN deduplication in sub-nodes
TEST_F(IResearchQueryOptimizationTest, test_185) {
  size_t optimizeType = 0;
  for (auto& o : optimizerOptionsAvailable) {
    SCOPED_TRACE(o);
    std::string const query =
      std::string("FOR d IN testView SEARCH  (d.values IN ['A', 'B', 'A']) AND  (d.values == 'A' OR d.values == 'C' OR d.values == 'C') ") +
      o + "RETURN d";
    EXPECT_TRUE(arangodb::tests::assertRules(vocbase(), query,
      { arangodb::aql::OptimizerRule::handleArangoSearchViewsRule }));

    EXPECT_FALSE(findEmptyNodes(vocbase(), query));
    // check structure only for non-optimized
    // Dnf-converter filter is out of scope, just run it and verify
    // returned documents are the same
    if (optimizeType >= disabledDnfOptimizationStart) {
      irs::Or expected;
      auto& andFilter = expected.add<irs::And>();
      auto& left = andFilter.add<irs::Or>();
      {
        auto& filter = left.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = left.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      }
      auto& right = andFilter.add<irs::Or>();
      {
        auto& filter = right.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      }
      {
        auto& filter = right.add<irs::by_term>();
        *filter.mutable_field() = mangleStringIdentity("values");
        filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      }

      assertFilterOptimized(vocbase(), query, expected);
    }
    std::vector<arangodb::velocypack::Slice> expectedDocs{
       arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase(), query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    ASSERT_EQ(expectedDocs.size(), resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      EXPECT_TRUE((0 == arangodb::basics::VelocyPackHelper::compare(
        arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
    ++optimizeType;
  }
}
